const path = require('path');
const Service = require('@vue/cli-service/lib/Service');
const manifest = require('../package.json');

// Web 构建保留 Vue 插件，只在内存中移除需要 Electron 二进制的打包插件。
const pkg = {
  ...manifest,
  dependencies: { ...manifest.dependencies },
  devDependencies: { ...manifest.devDependencies }
};
delete pkg.dependencies['vue-cli-plugin-electron-builder'];
delete pkg.devDependencies['vue-cli-plugin-electron-builder'];

const service = new Service(path.resolve(__dirname, '..'), { pkg });
service.run('build').catch(error => {
  console.error(error);
  process.exitCode = 1;
});
