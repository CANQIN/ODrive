#ifndef _TRAP_TRAJ_H
#define _TRAP_TRAJ_H

class TrapezoidalTrajectory {
public:
    struct Step_t {
        float Y;
        float Yd;
        float Ydd;
    };

    // Plan with finite inputs and strictly positive velocity/acceleration limits.
    bool planTrapezoidal(float Xf, float Xi, float Vi,
                         float Vmax, float Amax, float Dmax);
    // Evaluate a planned trajectory at finite t without advancing any clock.
    Step_t eval(float t) const;

    float Xi_;
    float Xf_;
    float Vi_;

    float Ar_;
    float Vr_;
    float Dr_;

    float Ta_;
    float Tv_;
    float Td_;
    float Tf_;

    float yAccel_;
};

#endif
