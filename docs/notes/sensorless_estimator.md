## 文件作用

`Firmware/MotorControl/sensorless_estimator.c/.h` 实现独立的 C99 无感估算算法。`Axis::update_sensorless_estimator()` 负责采样、端口有效性和错误上报。

| 接口数据 | 内容 |
| --- | --- |
| `sensorless_config_t` | 观测器增益、PLL 带宽、永磁磁链 |
| `sensorless_motor_t` | 相电阻、相电感、极对数 |
| `sensorless_input_t` | αβ 电流、电流有效标志、最新 αβ 电压反馈 |
| `sensorless_state_t` | 磁链、电压延迟、PLL 角度和上一拍角速度 |
| `sensorless_output_t` | 磁链角度、PLL 角度、电角速度、机械转速 |

调用 `sensorless_estimator_update()` 时显式传入上述输入、状态、配置和以秒为单位的 `dt`。状态由调用方按值持有，算法无堆分配、全局实例或硬件依赖。

算法流程如下：

```text
三相电流
  → Clarke 变换
  → 定子磁链积分
  → 非线性磁链观测器
  → 永磁体磁链方向
  → 电角度
  → PLL
  → 电角速度和机械转速
```

核心算法来自论文中的表贴式永磁同步电机非线性观测器。它不需要编码器，而是利用：

- 电机相电阻 \(R\)
- 电机相电感 \(L\)
- 永磁体磁链 \(\lambda_m\)
- 实际施加的 αβ 电压
- 测量的 αβ 电流

估算转子电角度和速度。

---

## 主要状态和参数

应用层为三个配置参数提供默认值，C 算法通过参数接收配置：

```cpp
float observer_gain = 1000.0f;
float pll_bandwidth = 1000.0f;
float pm_flux_linkage = 1.58e-3f;
```

含义如下：

- `observer_gain`：非线性磁链观测器增益。越大，磁链幅值收敛越快，但过大会放大噪声并可能造成离散系统振荡。
- `pll_bandwidth`：PLL 带宽，决定速度估算的响应速度和噪声。
- `pm_flux_linkage`：永磁体磁链，单位相当于 `V·s` 或 `Wb`。常用的 KV 换算公式为：

\[
\lambda_m \approx
\frac{5.5133}{p \cdot K_V}
\]

其中 \(p\) 是极对数，\(K_V\) 是 rpm/V。

内部状态包括：

- `state->flux[2]`：α、β 轴总定子磁链估计值。
- `state->voltage_memory[2]`：与当前电流采样时刻对应的历史施加电压。
- `state->pll_pos`：PLL 跟踪的电角度。
- `state->phase_vel`：上一拍电角速度，进入算法前由 Axis 同步端口有效值。
- `output->phase`：输出电角度，单位 rad。
- `output->phase_vel`：输出电角速度，单位 rad/s。
- `output->vel_estimate`：机械转速，单位 turns/s。

---

## `sensorless_estimator_reset()`：复位观测器

```cpp
void sensorless_estimator_reset(sensorless_state_t *state);
```

它把以下内部状态清零：

- PLL 角度
- PLL 角速度
- 历史 αβ 电压
- αβ 磁链积分状态

更新失败时，C 算法清零状态和输出并返回状态码。Axis 将状态码映射为原有通信错误位并锁存，同时清零机械速度和 PLL 角度；本周期不发布相位和角速度端口。错误仍由外部清除流程处理。

---

## `sensorless_estimator_update()`：一次估算过程

### 1. 计算 PLL 增益

```cpp
float pll_kp = 2.0f * config->pll_bandwidth;
float pll_ki = 0.25f * (pll_kp * pll_kp);
```

代入后：

\[
K_p = 2B
\]

\[
K_i = B^2
\]

其中 \(B\) 为 `pll_bandwidth`。这种配置对应临界阻尼二阶 PLL。

随后检查离散时间稳定条件：

```cpp
if (!(dt * pll_kp < 1.0f))
```

即：

\[
T_sK_p < 1
\]

不满足时复位状态和输出，返回 `SENSORLESS_UNSTABLE_GAIN`。Axis 锁存 `ERROR_UNSTABLE_GAIN`，控制循环记录 `ERROR_SENSORLESS_ESTIMATOR_FAILED`。

---

### 2. 获取三相电流

Axis 在临界区内一起读取三相电流、使能状态、最新电压反馈和电机参数，计算在临界区外执行。

未使能时，应用层提供有效的零电流。使能但采样无效时，`input->current_valid` 为 `false`，算法返回 `SENSORLESS_CURRENT_MISSING` 并复位状态；Axis 映射为 `ERROR_UNKNOWN_CURRENT_MEASUREMENT`。

增益稳定性检查先于电流有效性检查，保留原来的故障优先级。

---

### 3. Clarke 变换

```cpp
float I_alpha_beta[2] = {
    current_meas->phA,
    one_by_sqrt3 * (current_meas->phB - current_meas->phC)
};
```

即：

\[
I_\alpha = I_A
\]

\[
I_\beta = \frac{I_B-I_C}{\sqrt{3}}
\]

Axis 把三相静止坐标系电流转换为二维 αβ 电流，填入 `input.current[0]` 和 `input.current[1]`，算法不接触 ADC 或电机对象。

---

### 4. 根据电压方程预测总磁链

PMSM 在 αβ 坐标系中的电压方程为：

\[
\boldsymbol{v}
=
R\boldsymbol{i}
+
\frac{d\boldsymbol{\psi}_s}{dt}
\]

所以：

\[
\frac{d\boldsymbol{\psi}_s}{dt}
=
\boldsymbol{v}-R\boldsymbol{i}
\]

代码对应：

```cpp
float y =
    -phase_resistance * I_alpha_beta[i]
    + state->voltage_memory[i];

state->flux[i] += y * dt;
```

因此：

\[
\hat{\psi}_{s,k}
=
\hat{\psi}_{s,k-1}
+
T_s(v-Ri)
\]

这里的 `state->flux` 是总定子磁链，包含：

- 电感磁链 \(L i\)
- 永磁体磁链 \(\psi_m\)

---

### 5. 分离永磁体磁链

```cpp
eta[i] =
    state->flux[i]
    - phase_inductance * I_alpha_beta[i];
```

也就是：

\[
\boldsymbol{\eta}
=
\hat{\boldsymbol{\psi}}_s
-
L\boldsymbol{i}
\]

理论上：

\[
\boldsymbol{\eta}
\approx
\boldsymbol{\psi}_m
=
\lambda_m
\begin{bmatrix}
\cos\theta_e \\
\sin\theta_e
\end{bmatrix}
\]

所以：

- `eta` 的长度应该等于永磁体磁链 \(\lambda_m\)
- `eta` 的方向就是转子电角度

---

### 6. 非线性磁链校正

单纯对 \(v-Ri\) 积分会受到直流偏置、参数误差和测量误差影响，最终发生积分漂移。

代码通过已知的永磁体磁链长度进行约束：

```cpp
float pm_flux_sqr =
    config->pm_flux_linkage * config->pm_flux_linkage;

float est_pm_flux_sqr =
    eta[0] * eta[0] + eta[1] * eta[1];

float eta_factor =
    0.5f
    * (config->observer_gain / pm_flux_sqr)
    * (pm_flux_sqr - est_pm_flux_sqr);
```

对应校正项：

\[
\dot{\hat{\psi}}_{s,\mathrm{corr}}
=
\frac{\gamma}{2\lambda_m^2}
\left(
\lambda_m^2-\|\boldsymbol{\eta}\|^2
\right)
\boldsymbol{\eta}
\]

然后加入磁链状态：

```cpp
state->flux[i] +=
    eta_factor * eta[i] * dt;
```

其作用非常直观：

- 如果 \(\|\eta\| < \lambda_m\)，校正因子为正，增大磁链长度。
- 如果 \(\|\eta\| > \lambda_m\)，校正因子为负，减小磁链长度。
- 最终将永磁体磁链估计约束在半径为 \(\lambda_m\) 的圆附近。

这就是“非线性观测器”的核心。

---

### 7. 保存与电流采样对应的历史电压

```cpp
state->voltage_memory[0] = input->voltage[0];

state->voltage_memory[1] = input->voltage[1];
```

控制循环的调用顺序是：

```text
无感估算器
→ 速度/位置控制器
→ 电机控制
→ 电流控制器
```

因此执行估算器时，`final_v_alpha_` 和 `final_v_beta_` 还是上一控制周期产生的电压。再通过 `state->voltage_memory` 延迟一次，使电压与当前 ADC 电流采样的实际 PWM 周期对齐。

这是很重要的时序补偿。如果电压和电流错开一个周期，磁链积分会产生明显的相位和幅值误差。

---

### 8. 根据磁链方向计算电角度

```cpp
float phase = _sensorless_atan2(eta[1], eta[0]);
```

即：

\[
\theta_e = \operatorname{atan2}(\eta_\beta,\eta_\alpha)
\]

结果通常被限制在：

\[
[-\pi,\pi)
\]

这个角度就是估算出的转子电角度。

---

### 9. PLL 估算电角速度

首先使用上一周期速度预测 PLL 角度：

```cpp
float predicted =
    _sensorless_wrap_phase(state->pll_pos + dt * state->phase_vel);
```

然后计算观测角度与 PLL 角度之间的最短角度误差：

```cpp
float delta_phase =
    _sensorless_wrap_phase(phase - predicted);
```

`_sensorless_wrap_phase()` 很重要。例如：

- 当前角度接近 \(+\pi\)
- PLL 角度接近 \(-\pi\)

两者实际上只差很小，但直接相减会接近 \(2\pi\)。角度环绕可避免该问题。

PLL 更新为：

```cpp
state->pll_pos = _sensorless_wrap_phase(predicted + dt * pll_kp * delta_phase);
state->phase_vel += dt * pll_ki * delta_phase;
```

对应：

\[
\hat{\theta}_{PLL}
\leftarrow
\hat{\theta}_{PLL}
+
T_s\hat{\omega}
+
T_sK_p e_\theta
\]

\[
\hat{\omega}
\leftarrow
\hat{\omega}
+
T_sK_i e_\theta
\]

这样得到比直接对角度差分更平滑的电角速度。

一个值得注意的细节是：

```cpp
output->phase = phase;
```

输出给 FOC 的是 `atan2()` 得到的原始磁链角度，而不是 `output->pll_pos`。PLL 主要用于产生平滑的速度估算和下一周期相位预测。

---

### 10. 输出机械转速

```cpp
output->vel_estimate = state->phase_vel
    / (fmaxf((float)motor->pole_pairs, 1.0f) * 2.0f * 3.14159265358979323846f);
```

电角速度和机械速度的关系为：

\[
\omega_e=p\omega_m
\]

而机械转速以 turns/s 输出：

\[
n_m
=
\frac{\omega_e}{2\pi p}
\]

`fmaxf((float)motor->pole_pairs, 1.0f)` 用来防止极对数配置为零时发生除零。

---

## 在整个控制系统中的作用

进入无感闭环之前，ODrive 会先执行开环启动 `sensorless_ramp`，默认最终电角速度约为：

```cpp
400 rad/s
```

完成启动后：

- `phase_` 连接到 FOC 电流环的电角度输入。
- `phase_vel_` 连接到电机前馈和角速度输入。
- `vel_estimate_` 连接到速度控制器。
- 位置输入断开，因此该模式主要用于速度控制和转矩控制，不能提供可靠的绝对机械位置控制。

之所以必须先开环加速，是因为该观测器在零速、极低速时可观测性较差。低速时反电动势很小，电阻压降、死区误差和测量偏置会占主导。

## 需要特别注意的地方

1. `phase_resistance`、`phase_inductance` 和 `pm_flux_linkage` 必须准确，否则磁链估算会产生系统性误差。

2. 代码没有检查 `pm_flux_linkage == 0`，错误配置会在：

```cpp
1.0f / pm_flux_sqr
```

处产生无穷大或 NaN。

3. 只检查了 PLL 增益的离散稳定性，没有检查 `observer_gain`。观测器增益过大同样可能导致振荡。

4. 算法假设单一的 `phase_inductance`，更适合表贴式 PMSM。对于 \(L_d \neq L_q\) 的强凸极 IPMSM，模型误差会更明显。

5. 使用的是控制器计算出的最终施加电压，而不是真正测量的端电压，因此 PWM 死区、器件压降和母线波动仍会形成观测误差。

总体而言，这个文件完成的就是：通过电机电压方程重建永磁体磁链，用磁链方向获取电角度，再通过 PLL 得到电角速度和机械转速。