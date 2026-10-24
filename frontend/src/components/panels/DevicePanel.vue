<template>
  <div class="modal-backdrop" @click.self="emit('close')">
    <section class="modal-dialog" role="dialog" aria-modal="true" aria-labelledby="device-dialog-title">
      <header class="dialog-header">
        <div>
          <strong id="device-dialog-title">设备连接</strong>
        </div>
        <button type="button" title="关闭" @click="emit('close')">
          <i class="ri-close-line"></i>
        </button>
      </header>
      <div class="device-list">
        <div class="device-panel">
          <div class="panel-title">
            <span><i class="ri-serial-port-line"></i> MINIMTS 控制器</span>
          </div>
          <div class="select-row">
            <select v-model="selectedMINIMTS" class="device-select" :disabled="minimtsConnected || minimtsBusy">
              <option value="">{{ minimtsDevices.length ? '请选择串口' : '未发现串口' }}</option>
              <option v-for="device in minimtsDevices" :key="device" :value="device">{{ device }}</option>
            </select>
            <button class="refresh-btn" type="button" title="刷新串口列表" :disabled="minimtsLoading || minimtsBusy" @click="loadMINIMTSDevices">
              <i class="ri-refresh-line" :class="{ spinning: minimtsLoading }"></i>
            </button>
          </div>
          <button
            class="connect-btn"
            :class="{ connected: minimtsConnected }"
            type="button"
            :disabled="minimtsBusy || (!minimtsConnected && !selectedMINIMTS)"
            @click="toggleMINIMTS"
          >
            {{ minimtsBusy ? '处理中...' : minimtsConnected ? '断开设备' : '连接设备' }}
          </button>
        </div>

        <div class="device-panel">
          <div class="panel-title">
            <span><i class="ri-camera-line"></i> 视频引伸计</span>
          </div>
          <div class="select-row">
            <select v-model="selectedCamera" class="device-select" :disabled="cameraConnected || cameraBusy">
              <option value="">{{ cameraDevices.length ? '请选择相机' : '未发现相机' }}</option>
              <option v-for="device in cameraDevices" :key="device" :value="device">{{ device }}</option>
            </select>
            <button class="refresh-btn" type="button" title="刷新相机列表" :disabled="cameraLoading || cameraBusy" @click="loadCameraDevices">
              <i class="ri-refresh-line" :class="{ spinning: cameraLoading }"></i>
            </button>
          </div>
          <button
            class="connect-btn"
            :class="{ connected: cameraConnected }"
            type="button"
            :disabled="cameraBusy || (!cameraConnected && !selectedCamera)"
            @click="toggleCamera"
          >
            {{ cameraBusy ? '处理中...' : cameraConnected ? '断开相机' : '连接相机' }}
          </button>
        </div>
      </div>
    </section>
  </div>
</template>

<script setup>
import { inject, onMounted, ref } from 'vue';
import {
  CloseMINIMTS,
  GetMINIMTSDevices,
  IsMINIMTSConnected,
  OpenMINIMTS,
} from '../../../bindings/MINIMTS/services/devices/MINIMTSService.js';
import {
  CloseHIKCamera,
  GetHIKCameraDevices,
  IsHIKCameraConnected,
  OpenHIKCamera,
} from '../../../bindings/MINIMTS/services/devices/hikcameraservice.js';

const emit = defineEmits(['status-change', 'close']);
const toast = inject('toast', () => {});

const minimtsDevices = ref([]);
const cameraDevices = ref([]);
const selectedMINIMTS = ref('');
const selectedCamera = ref('');
const minimtsConnected = ref(false);
const cameraConnected = ref(false);
const minimtsLoading = ref(false);
const cameraLoading = ref(false);
const minimtsBusy = ref(false);
const cameraBusy = ref(false);

const errorText = (error) => error?.message || String(error || '操作失败');

const notifyStatus = () => {
  emit('status-change', {
    minimtsConnected: minimtsConnected.value,
    cameraConnected: cameraConnected.value,
  });
};

const loadMINIMTSDevices = async () => {
  minimtsLoading.value = true;
  try {
    minimtsDevices.value = (await GetMINIMTSDevices()) || [];
    if (minimtsConnected.value && selectedMINIMTS.value && !minimtsDevices.value.includes(selectedMINIMTS.value)) {
      minimtsDevices.value.unshift(selectedMINIMTS.value);
    } else if (!minimtsConnected.value && !minimtsDevices.value.includes(selectedMINIMTS.value)) {
      selectedMINIMTS.value = minimtsDevices.value[0] || '';
    }
  } catch (error) {
    minimtsDevices.value = [];
    selectedMINIMTS.value = '';
    toast(errorText(error), 'error');
  } finally {
    minimtsLoading.value = false;
  }
};

const loadCameraDevices = async () => {
  cameraLoading.value = true;
  try {
    cameraDevices.value = (await GetHIKCameraDevices()) || [];
    if (cameraConnected.value && selectedCamera.value && !cameraDevices.value.includes(selectedCamera.value)) {
      cameraDevices.value.unshift(selectedCamera.value);
    } else if (!cameraConnected.value && !cameraDevices.value.includes(selectedCamera.value)) {
      selectedCamera.value = cameraDevices.value[0] || '';
    }
  } catch (error) {
    cameraDevices.value = [];
    selectedCamera.value = '';
    toast(errorText(error), 'error');
  } finally {
    cameraLoading.value = false;
  }
};

const toggleMINIMTS = async () => {
  minimtsBusy.value = true;
  try {
    if (minimtsConnected.value) {
      await CloseMINIMTS();
    } else {
      await OpenMINIMTS(selectedMINIMTS.value);
    }
    minimtsConnected.value = await IsMINIMTSConnected();
    notifyStatus();
  } catch (error) {
    toast(errorText(error), 'error');
  } finally {
    minimtsBusy.value = false;
  }
};

const toggleCamera = async () => {
  cameraBusy.value = true;
  try {
    if (cameraConnected.value) {
      await CloseHIKCamera();
    } else {
      await OpenHIKCamera(selectedCamera.value);
    }
    cameraConnected.value = await IsHIKCameraConnected();
    notifyStatus();
  } catch (error) {
    toast(errorText(error), 'error');
  } finally {
    cameraBusy.value = false;
  }
};

onMounted(async () => {
  const results = await Promise.allSettled([
    IsMINIMTSConnected(),
    IsHIKCameraConnected(),
  ]);
  minimtsConnected.value = results[0].status === 'fulfilled' && results[0].value;
  cameraConnected.value = results[1].status === 'fulfilled' && results[1].value;
  notifyStatus();

  await loadMINIMTSDevices();
  await loadCameraDevices();
});
</script>

<style scoped>
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
  width: min(520px, calc(100vw - 32px));
  max-height: calc(100vh - 32px);
  padding: 18px;
  background: #0f172a;
  border: 1px solid #475569;
  border-radius: 8px;
  box-shadow: 0 24px 70px rgba(0, 0, 0, 0.45);
  overflow: auto;
}

.dialog-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 20px;
  margin-bottom: 18px;
}

.dialog-header strong {
  color: #f8fafc;
  font-size: 16px;
}

.dialog-header button {
  width: 34px;
  height: 34px;
  flex: 0 0 auto;
  margin: 0;
  padding: 0;
  display: grid;
  place-items: center;
  color: #94a3b8;
  background: transparent;
  border: none;
  border-radius: 6px;
  font-size: 20px;
  cursor: pointer;
}

.dialog-header button:hover {
  color: #f8fafc;
  background: rgba(148, 163, 184, 0.14);
}

.device-panel {
  background: #1e293b;
  border-radius: 8px;
  padding: 16px;
  border: 1px solid #334155;
  display: flex;
  flex-direction: column;
  align-items: center;
}

.device-panel + .device-panel {
  margin-top: 16px;
}

.panel-title {
  display: flex;
  justify-content: center;
  align-items: center;
  margin-bottom: 12px;
  font-size: 14px;
  font-weight: 500;
  color: #f8fafc;
}

.panel-title span {
  display: flex;
  align-items: center;
  gap: 8px;
}

.panel-title i {
  color: #3b82f6;
  font-size: 18px;
}

.select-row {
  display: flex;
  gap: 8px;
  width: 100%;
  margin-bottom: 12px;
  align-items: center;
}

.device-select {
  flex: 1;
  min-width: 0;
  height: 42px;
  padding: 0 12px;
  background: #0f172a;
  border: 1px solid #334155;
  border-radius: 8px;
  color: #f8fafc;
  outline: none;
  font-size: 14px;
  cursor: pointer;
  appearance: none;
  background-image: url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='16' height='16' viewBox='0 0 24 24' fill='none' stroke='%23f8fafc' stroke-width='2' stroke-linecap='round' stroke-linejoin='round'%3E%3Cpolyline points='6 9 12 15 18 9'%3E%3C/polyline%3E%3C/svg%3E");
  background-repeat: no-repeat;
  background-position: right 12px center;
}

.device-select option {
  background: #0f172a;
  color: #f8fafc;
}

.device-select:disabled {
  cursor: not-allowed;
  opacity: 0.55;
}

.refresh-btn {
  background: rgba(255, 255, 255, 0.05);
  border: 1px solid #334155;
  color: #94a3b8;
  cursor: pointer;
  padding: 4px;
  border-radius: 8px;
  transition: all 0.2s;
  width: 42px;
  height: 42px;
  flex-shrink: 0;
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 18px;
}

.refresh-btn:hover:not(:disabled) {
  background: rgba(59, 130, 246, 0.15);
  border-color: rgba(59, 130, 246, 0.4);
  color: #f8fafc;
}

.refresh-btn:disabled {
  cursor: not-allowed;
  opacity: 0.5;
}

.refresh-btn i {
  display: inline-block;
}

.refresh-btn .spinning {
  animation: spin 1s linear infinite;
}

@keyframes spin {
  from { transform: rotate(0deg); }
  to { transform: rotate(360deg); }
}

.connect-btn {
  width: 90%;
  height: 50px;
  padding: 12px;
  background: #3b82f6;
  color: white;
  border: none;
  border-radius: 8px;
  font-size: 14px;
  font-weight: 600;
  cursor: pointer;
  transition: all 0.2s;
}

.connect-btn:hover:not(:disabled) {
  background: #2563eb;
}

.connect-btn:disabled {
  background: #64748b;
  cursor: not-allowed;
}

.connect-btn.connected {
  background: #10b981;
}

.connect-btn.connected:hover:not(:disabled) {
  background: #059669;
}
</style>
