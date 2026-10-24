<template>
  <div class="minimts-shell">
    <header class="title-bar" @dblclick="toggleMaximiseWindow">
      <div class="brand-logo">
        <img src="../res/logo blue.png" alt="MINIMTS PRO">
      </div>
      <div class="app-title">
        <strong>MINIMTS PRO</strong>
        <span>微型材料试验系统</span>
      </div>
      <div class="window-actions">
        <button
          class="conn-status"
          :class="{ connected: minimtsConnected }"
          type="button"
          :title="minimtsConnected ? '设备已连接' : '设备未连接'"
          @click="openDevicePanel"
        >
          <i class="ri-link-m"></i>
          <em></em>
        </button>
        <button class="window-btn avatar-btn" type="button" title="用户"><i class="ri-account-circle-line"></i></button>
        <button class="window-btn" type="button" title="系统设置" @click="openSystemPanel">
          <i class="ri-settings-3-line"></i>
        </button>
        <span class="title-divider" aria-hidden="true"></span>
        <button class="window-btn" type="button" title="最小化" @click="minimiseWindow"><i class="ri-subtract-line"></i></button>
        <button class="window-btn" type="button" title="最大化/还原" @click="toggleMaximiseWindow"><i class="ri-checkbox-blank-line"></i></button>
        <button class="window-btn close-btn" type="button" title="关闭" @click="quitApplication"><i class="ri-close-line"></i></button>
      </div>
    </header>

    <div class="content">
      <aside class="tool-bar" aria-label="工具栏">
        <button class="tool-button connect-btn" :class="{ connected: minimtsConnected }" type="button" title="设备连接" @click="openDevicePanel">
          <i class="ri-link-m"></i>
        </button>
        <button v-if="cameraConnected" class="tool-button camera-btn connected" type="button" title="相机" @click="openCameraWindow">
          <i class="ri-camera-line"></i>
        </button>
        <button class="tool-button" type="button" title="新建项目" @click="openProjectPanel">
          <i class="ri-file-add-line"></i>
        </button>
        <button class="tool-button" type="button" title="保存文件">
          <i class="ri-save-3-line"></i>
        </button>
        <button class="tool-button" type="button" title="文件管理">
          <i class="ri-folder-settings-line"></i>
        </button>
      </aside>

      <main class="data-area">
        <section class="metrics-grid" aria-label="实时数据显示">
          <MetricCard label="应力" :value="metrics.stress" unit="MPa" sub-label="载荷" :sub-value="metrics.load" sub-unit="N" accent="#f59e0b" />
          <MetricCard label="应变" :value="metrics.strain" unit="ε" sub-label="位移" :sub-value="metrics.disp" sub-unit="mm" accent="#8b5cf6" />
          <MetricCard label="视频应变" :value="metrics.eStrain" unit="ε" sub-label="视频位移" :sub-value="metrics.ePos" sub-unit="pixels" accent="#06b6d4" />
          <MetricCard label="时间" :value="metrics.time" unit="s" sub-label="北京时间" :sub-value="metrics.clock" accent="#ec4899" />
        </section>

        <section class="workspace">
          <TrendPanel />
          <ControlPanel />
        </section>
      </main>
    </div>

    <div v-if="devicePanelInitialized" v-show="devicePanelVisible" class="modal-backdrop" @click.self="closeDevicePanel">
      <section class="modal-dialog" role="dialog" aria-modal="true" aria-labelledby="device-dialog-title">
        <header class="dialog-header">
          <div>
            <strong id="device-dialog-title">设备连接</strong>
            <span>选择并管理试验机与视频引伸计</span>
          </div>
          <button type="button" title="关闭" @click="closeDevicePanel">
            <i class="ri-close-line"></i>
          </button>
        </header>
        <DevicePanel @status-change="updateDeviceStatus" />
      </section>
    </div>

    <div v-if="systemPanelVisible" class="modal-backdrop" @click.self="closeSystemPanel">
      <section class="modal-dialog system-dialog" role="dialog" aria-modal="true" aria-labelledby="system-dialog-title">
        <header class="dialog-header">
          <div>
            <strong id="system-dialog-title">系统设置</strong>
            <span>配置控制器与传感器模块参数</span>
          </div>
          <button type="button" title="关闭" @click="closeSystemPanel">
            <i class="ri-close-line"></i>
          </button>
        </header>
        <SystemPanel @close="closeSystemPanel" @save="closeSystemPanel" />
      </section>
    </div>

    <div v-if="projectPanelVisible" class="modal-backdrop" @click.self="closeProjectPanel">
      <section class="modal-dialog project-dialog" role="dialog" aria-modal="true" aria-labelledby="project-dialog-title">
        <header class="dialog-header">
          <div>
            <strong id="project-dialog-title">项目设置</strong>
            <span>配置实验参数、样品尺寸与视频引伸计</span>
          </div>
          <button type="button" title="关闭" @click="closeProjectPanel">
            <i class="ri-close-line"></i>
          </button>
        </header>
        <ProjectPanel :is-camera-connected="cameraConnected" @cancel="closeProjectPanel" @submit="closeProjectPanel" />
      </section>
    </div>

    <footer class="footer">
      <div class="status-bar">
        <span class="status-item" title="CPU 利用率">
          <i class="ri-cpu-line"></i>
          <em>{{ pcStatus.cpu }}%</em>
        </span>
        <span class="status-item" title="内存占用">
          <i class="ri-database-2-line"></i>
          <em>{{ pcStatus.memory }}%</em>
        </span>
        <span class="status-item" title="磁盘使用">
          <i class="ri-hard-drive-2-line"></i>
          <em>{{ pcStatus.hdd }}%</em>
        </span>
        <span class="status-item" title="网络状态">
          <i class="ri-signal-wifi-line"></i>
          <em>在线</em>
        </span>
        <span class="status-item" title="串口通信">
          <i class="ri-serial-port-line"></i>
          <em>COM3</em>
        </span>
        <span class="status-item alert" title="告警信息">
          <i class="ri-error-warning-line"></i>
          <em>0</em>
        </span>
        <span class="status-item spacer"></span>
        <span class="status-item" title="系统时间">
          <i class="ri-time-line"></i>
          <em>{{ metrics.clock }}</em>
        </span>
      </div>
    </footer>

    <div class="toast-container" role="status" aria-live="polite">
      <div v-for="toast in toasts" :key="toast.id" class="toast" :class="toast.type">
        <i :class="toastIcon(toast.type)"></i>
        <span>{{ toast.text }}</span>
        <button type="button" title="关闭" @click="dismissToast(toast.id)">
          <i class="ri-close-line"></i>
        </button>
      </div>
    </div>
  </div>
</template>

<script setup>
import { onBeforeUnmount, onMounted, provide, reactive, ref } from 'vue';
import { Application, Events, Window } from '@wailsio/runtime';
import { IsMINIMTSConnected, MotorJog, MotorStop, StartMeasurement, StopMeasurement} from '../../bindings/MINIMTS/services/minimtsservice.js';
import { IsHIKCameraConnected, CallCameraWindow } from '../../bindings/MINIMTS/services/hikcameraservice.js';
import MetricCard from '../panels/MetricCard.vue';
import TrendPanel from '../panels/TrendPanel.vue';
import ControlPanel from '../panels/ControlPanel.vue';
import DevicePanel from '../panels/DevicePanel.vue';
import SystemPanel from '../panels/SystemPanel.vue';
import ProjectPanel from '../panels/ProjectPanel.vue';

const minimiseWindow = () => Window.Minimise();
const toggleMaximiseWindow = () => Window.ToggleMaximise();
const quitApplication = () => Application.Quit();

const devicePanelVisible = ref(false);
const devicePanelInitialized = ref(false);
const systemPanelVisible = ref(false);
const projectPanelVisible = ref(false);
const minimtsConnected = ref(false);
const cameraConnected = ref(false);
const pcStatus = reactive({
  cpu: '0.0',
  memory: '0.0',
  hdd: '0.0',
});
const metrics = reactive({
  stress: '0.0',
  load: '0.0',
  strain: '0.0000',
  disp: '0.000',
  eStrain: '0.0000',
  ePos: '0.000',
  time: '0.000',
  clock: '--:--:--',
});
let unsubscribePCHeartbeat = null;
let unsubscribeMTSHeartbeat = null;

const TOAST_DURATION = 1500;
const toastIcons = {
  error: 'ri-error-warning-line',
  success: 'ri-checkbox-circle-line',
  info: 'ri-information-line',
};
const toastIcon = (type) => toastIcons[type] || toastIcons.info;

const toasts = ref([]);
let toastSeed = 0;

const dismissToast = (id) => {
  toasts.value = toasts.value.filter((item) => item.id !== id);
};

const showToast = (message, type = 'info') => {
  const text = String(message ?? '').trim();
  if (!text) return;

  const id = ++toastSeed;
  toasts.value.push({ id, text, type: toastIcons[type] ? type : 'info' });
  setTimeout(() => dismissToast(id), TOAST_DURATION);
};

provide('toast', showToast);

const openDevicePanel = () => {
  devicePanelInitialized.value = true;
  devicePanelVisible.value = true;
};
const openCameraWindow = async () => {
  try {
    await CallCameraWindow();
  } catch (error) {
    showToast(error?.message || String(error || '打开相机窗口失败'), 'error');
  }
};
const closeDevicePanel = () => { devicePanelVisible.value = false; };
const openSystemPanel = () => { systemPanelVisible.value = true; };
const closeSystemPanel = () => { systemPanelVisible.value = false; };
const openProjectPanel = () => { projectPanelVisible.value = true; };
const closeProjectPanel = () => { projectPanelVisible.value = false; };
const updateDeviceStatus = (status) => {
  minimtsConnected.value = status.minimtsConnected;
  cameraConnected.value = status.cameraConnected;
};

const updatePCStatus = (data) => {
  pcStatus.cpu = Number(data.cpu || 0).toFixed(1);
  pcStatus.memory = Number(data.memory || 0).toFixed(1);
  pcStatus.hdd = Number(data.hdd || 0).toFixed(1);

};

const updateMetrics = (data) => {
  metrics.stress = Number(data.stress || 0).toFixed(1);
  metrics.load = Number(data.load || 0).toFixed(1);
  metrics.strain = Number(data.strain || 0).toFixed(4);
  metrics.disp = Number(data.disp || 0).toFixed(3);
  metrics.eStrain = Number(data.eStrain || 0).toFixed(4);
  metrics.ePos = Number(data.ePos || 0).toFixed(3);
  metrics.time = Number(data.time || 0).toFixed(3);

  const startTime = new Date(data.startTime);
  const hasStartTime = !Number.isNaN(startTime.getTime()) && startTime.getFullYear() > 2000;
  const clock = hasStartTime
    ? new Date(startTime.getTime() + Number(data.time || 0) * 1000)
    : new Date();
  metrics.clock = clock.toLocaleTimeString('zh-CN', { hour12: false });
};

onMounted(async () => {
  unsubscribePCHeartbeat = Events.On('pc-heartbeat', (event) => updatePCStatus(event.data));
  unsubscribeMTSHeartbeat = Events.On('mts-heartbeat', (event) => updateMetrics(event.data));
  const results = await Promise.allSettled([
    IsMINIMTSConnected(),
    IsHIKCameraConnected(),
  ]);
  minimtsConnected.value = results[0].status === 'fulfilled' && results[0].value;
  cameraConnected.value = results[1].status === 'fulfilled' && results[1].value;
});

onBeforeUnmount(() => {
  unsubscribePCHeartbeat?.();
  unsubscribeMTSHeartbeat?.();
});
</script>

<style scoped>
@import 'remixicon/fonts/remixicon.css';

.minimts-shell {
  --page-bg: #1e293b;
  --title-bg: #0f172a;
  --panel-bg: #273549;
  --button-bg: #334155;
  --border: #475569;
  --text-main: #f8fafc;
  --text-normal: #cbd5e1;
  --text-muted: #94a3b8;
  --primary: #3b82f6;
  position: fixed;
  inset: 0;
  display: grid;
  grid-template-rows: 58px minmax(0, 1fr) 34px;
  min-width: 1024px;
  min-height: 640px;
  color: var(--text-normal);
  background: var(--page-bg);
  overflow: hidden;
}

.title-bar {
  min-width: 0;
  display: grid;
  grid-template-columns: 74px minmax(0, 1fr) auto;
  align-items: center;
  background: var(--title-bg);
  border-bottom: 1px solid var(--border);
  user-select: none;
  --wails-draggable: drag;
}

.brand-logo {
  height: 100%;
  display: grid;
  place-items: center;
  /* border-right: 1px solid rgba(71, 85, 105, 0.65); */
  --wails-draggable: drag;
}

.brand-logo img {
  width: 30px;
  height: 30px;
  display: block;
  object-fit: contain;
  border-radius: 7px;
  pointer-events: none;
}

.app-title {
  min-width: 0;
  padding: 0 16px;
  display: flex;
  align-items: baseline;
  gap: 10px;
  --wails-draggable: drag;
}

.app-title strong {
  color: var(--text-main);
  font-size: 17px;
  font-weight: 700;
}

.app-title span {
  color: var(--text-muted);
  font-size: 12px;
}

.window-actions {
  align-self: stretch;
  display: flex;
  align-items: center;
  justify-content: flex-end;
  gap: 4px;
  padding: 0 10px;
  --wails-draggable: no-drag;
}

.window-actions .window-btn {
  width: 36px;
  height: 36px;
  margin: 0;
  padding: 0;
  display: flex;
  align-items: center;
  justify-content: center;
  color: var(--text-muted);
  background: transparent;
  border: none;
  border-radius: 8px;
  font-size: 18px;
  cursor: pointer;
  transition: color 0.2s ease, background-color 0.2s ease, transform 0.2s ease;
}

.window-actions .window-btn:hover {
  color: var(--text-main);
  background: rgba(148, 163, 184, 0.14);
  transform: scale(1.1);
}

.window-actions .conn-status {
  position: relative;
  width: 36px;
  height: 36px;
  margin: 0;
  padding: 0;
  display: flex;
  align-items: center;
  justify-content: center;
  color: var(--text-muted);
  background: transparent;
  border: none;
  border-radius: 8px;
  font-size: 18px;
  cursor: pointer;
  transition: color 0.2s ease, background-color 0.2s ease;
  --wails-draggable: no-drag;
}

.window-actions .conn-status:hover {
  color: var(--text-main);
  background: rgba(148, 163, 184, 0.14);
}

.window-actions .conn-status em {
  position: absolute;
  top: 7px;
  right: 7px;
  width: 7px;
  height: 7px;
  background: #ef4444;
  border: 1px solid var(--title-bg);
  border-radius: 50%;
  transition: background 0.2s ease;
}

.window-actions .conn-status.connected {
  color: #4ade80;
}

.window-actions .conn-status.connected em {
  background: #4ade80;
}

.window-actions .avatar-btn {
  color: var(--primary);
}

.window-actions .avatar-btn:hover {
  color: #60a5fa;
  background: rgba(59, 130, 246, 0.16);
}

.title-divider {
  width: 1px;
  height: 18px;
  margin: 0 6px;
  flex: 0 0 auto;
  background: rgba(148, 163, 184, 0.55);
}

.window-actions .close-btn:hover {
  color: #f87171;
  background: rgba(239, 68, 68, 0.16);
}

.content {
  min-height: 0;
  display: grid;
  grid-template-columns: 64px minmax(0, 1fr);
  overflow: hidden;
}

.tool-bar {
  padding: 14px 8px;
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 8px;
  background: #182337;
  border-right: 1px solid var(--border);
}

.tool-button {
  width: 44px;
  height: 44px;
  margin: 0;
  padding: 0;
  display: flex;
  align-items: center;
  justify-content: center;
  color: var(--text-muted);
  background: transparent;
  border: none;
  border-radius: 8px;
  font-size: 20px;
  cursor: pointer;
  transition: color 0.2s ease, background-color 0.2s ease, transform 0.2s ease;
}

.tool-button:hover {
  color: var(--text-main);
  background: rgba(148, 163, 184, 0.14);
  transform: scale(1.1);
}

.tool-button.connect-btn.connected {
  color: #4ade80;
}

.tool-button.connect-btn.connected:hover {
  color: #4ade80;
  background: rgba(74, 222, 128, 0.15);
}

.tool-button.camera-btn.connected {
  color: #38bdf8;
}

.modal-backdrop {
  position: fixed;
  inset: 0;
  z-index: 1000;
  display: grid;
  place-items: center;
  padding: 16px;
  background: rgba(2, 6, 23, 0.72);
  --wails-draggable: no-drag;
}

.modal-dialog {
  width: min(760px, calc(100vw - 32px));
  max-height: calc(100vh - 32px);
  padding: 18px;
  background: #0f172a;
  border: 1px solid #475569;
  border-radius: 8px;
  box-shadow: 0 24px 70px rgba(0, 0, 0, 0.45);
  overflow: auto;
}

.system-dialog {
  width: min(720px, calc(100vw - 32px));
}

.project-dialog {
  width: min(1100px, calc(100vw - 32px));
}

.dialog-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 20px;
  margin-bottom: 18px;
}

.dialog-header div {
  min-width: 0;
  display: flex;
  flex-direction: column;
  gap: 3px;
}

.dialog-header strong {
  color: var(--text-main);
  font-size: 16px;
}

.dialog-header span {
  color: var(--text-muted);
  font-size: 12px;
}

.dialog-header button {
  width: 34px;
  height: 34px;
  flex: 0 0 auto;
  margin: 0;
  padding: 0;
  display: grid;
  place-items: center;
  color: var(--text-muted);
  background: transparent;
  border: none;
  border-radius: 6px;
  font-size: 20px;
  cursor: pointer;
}

.dialog-header button:hover {
  color: var(--text-main);
  background: rgba(148, 163, 184, 0.14);
}

.data-area {
  min-width: 960px;
  min-height: 0;
  display: flex;
  flex-direction: column;
  overflow: hidden;
}

.metrics-grid {
  flex-shrink: 0;
  display: grid;
  grid-template-columns: repeat(4, minmax(0, 1fr));
  gap: 12px;
  padding: 14px 16px;
}

.workspace {
  flex: 1;
  min-height: 0;
  display: grid;
  grid-template-columns: minmax(0, 1fr) 250px;
  gap: 14px;
  padding: 0 16px 14px;
}

.footer {
  background: var(--title-bg);
  border-top: 1px solid var(--border);
}

.status-bar {
  height: 100%;
  display: flex;
  align-items: center;
  gap: 18px;
  padding: 0 16px;
  font-size: 11px;
  color: var(--text-muted);
  user-select: none;
}

.status-item {
  display: flex;
  align-items: center;
  gap: 5px;
  white-space: nowrap;
}

.status-item i {
  font-size: 14px;
}

.status-item em {
  font-style: normal;
  font-variant-numeric: tabular-nums;
}

.status-item.spacer {
  flex: 1;
}

.status-item.alert i {
  color: #fbbf24;
}

.status-item.alert em {
  color: var(--text-normal);
}

.toast-container {
  position: fixed;
  top: 70px;
  right: 16px;
  z-index: 2000;
  width: min(340px, calc(100vw - 32px));
  display: flex;
  flex-direction: column;
  gap: 8px;
  pointer-events: none;
}

.toast {
  display: grid;
  grid-template-columns: auto minmax(0, 1fr) auto;
  align-items: center;
  gap: 8px;
  padding: 10px 10px 10px 12px;
  color: #e2e8f0;
  background: #1e293b;
  border: 1px solid var(--border);
  border-left: 3px solid var(--primary);
  border-radius: 6px;
  box-shadow: 0 12px 30px rgba(0, 0, 0, 0.45);
  font-size: 12px;
  pointer-events: auto;
  animation: toast-in 180ms ease-out;
}

.toast span {
  min-width: 0;
  overflow-wrap: anywhere;
}

.toast > i {
  color: #60a5fa;
  font-size: 16px;
}

.toast.error {
  border-left-color: #ef4444;
}

.toast.error > i {
  color: #f87171;
}

.toast.success {
  border-left-color: #10b981;
}

.toast.success > i {
  color: #34d399;
}

.toast button {
  width: 22px;
  height: 22px;
  flex: 0 0 auto;
  margin: 0;
  padding: 0;
  display: grid;
  place-items: center;
  color: var(--text-muted);
  background: transparent;
  border: none;
  border-radius: 4px;
  font-size: 15px;
  cursor: pointer;
}

.toast button:hover {
  color: var(--text-main);
  background: rgba(148, 163, 184, 0.18);
}

@keyframes toast-in {
  from {
    opacity: 0;
    transform: translateX(16px);
  }

  to {
    opacity: 1;
    transform: translateX(0);
  }
}

</style>
