<template>
  <div class="device-grid">
    <section>
      <div class="device-title">
        <span><i class="ri-cpu-line"></i> MINIMTS 控制器</span>
        <em :class="{ connected: minimtsConnected }">
          {{ minimtsConnected ? '已连接' : '未连接' }}
        </em>
      </div>
      <label for="minimts-device">通信端口</label>
      <div class="select-row">
        <select id="minimts-device" v-model="selectedMINIMTS" :disabled="minimtsConnected || minimtsBusy">
          <option value="">{{ minimtsDevices.length ? '请选择串口' : '未发现串口' }}</option>
          <option v-for="device in minimtsDevices" :key="device" :value="device">{{ device }}</option>
        </select>
        <button class="refresh-button" type="button" title="刷新串口" :disabled="minimtsLoading || minimtsBusy" @click="loadMINIMTSDevices">
          <i class="ri-loop-left-line" :class="{ spinning: minimtsLoading }"></i>
        </button>
      </div>
      <button
        class="connect"
        :class="{ disconnect: minimtsConnected }"
        type="button"
        :disabled="minimtsBusy || (!minimtsConnected && !selectedMINIMTS)"
        @click="toggleMINIMTS"
      >
        <i :class="minimtsConnected ? 'ri-link-unlink-m' : 'ri-link-m'"></i>
        {{ minimtsBusy ? '处理中...' : minimtsConnected ? '断开设备' : '连接设备' }}
      </button>
    </section>

    <section>
      <div class="device-title">
        <span><i class="ri-camera-lens-line"></i> 视频引伸计</span>
        <em :class="{ connected: cameraConnected }">
          {{ cameraConnected ? '已连接' : '未连接' }}
        </em>
      </div>
      <label for="camera-device">相机设备</label>
      <div class="select-row">
        <select id="camera-device" v-model="selectedCamera" :disabled="cameraConnected || cameraBusy">
          <option value="">{{ cameraDevices.length ? '请选择相机' : '未发现相机' }}</option>
          <option v-for="device in cameraDevices" :key="device" :value="device">{{ device }}</option>
        </select>
        <button class="refresh-button" type="button" title="刷新相机" :disabled="cameraLoading || cameraBusy" @click="loadCameraDevices">
          <i class="ri-loop-left-line" :class="{ spinning: cameraLoading }"></i>
        </button>
      </div>
      <button
        class="connect"
        :class="{ disconnect: cameraConnected }"
        type="button"
        :disabled="cameraBusy || (!cameraConnected && !selectedCamera)"
        @click="toggleCamera"
      >
        <i :class="cameraConnected ? 'ri-link-unlink-m' : 'ri-link-m'"></i>
        {{ cameraBusy ? '处理中...' : cameraConnected ? '断开设备' : '连接设备' }}
      </button>
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
} from '../../bindings/MINIMTS/services/minimtsservice.js';
import {
  CloseHIKCamera,
  GetHIKCameraDevices,
  IsHIKCameraConnected,
  OpenHIKCamera,
} from '../../bindings/MINIMTS/services/hikcameraservice.js';

const emit = defineEmits(['status-change']);
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
.device-grid {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 16px;
}

section {
  min-width: 0;
  padding: 18px;
  background: #1e293b;
  border: 1px solid #475569;
  border-radius: 7px;
}

.device-title {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 12px;
  margin-bottom: 18px;
  color: #f8fafc;
  font-weight: 600;
}

.device-title span {
  display: flex;
  gap: 8px;
  align-items: center;
}

.device-title i {
  color: #3b82f6;
  font-size: 20px;
}

.device-title em {
  flex: 0 0 auto;
  padding: 4px 8px;
  color: #f87171;
  background: rgba(239, 68, 68, 0.1);
  border-radius: 12px;
  font-size: 11px;
  font-style: normal;
}

.device-title em.connected {
  color: #4ade80;
  background: rgba(74, 222, 128, 0.12);
}

label {
  display: block;
  margin-bottom: 7px;
  color: #94a3b8;
  font-size: 12px;
}

.select-row {
  display: grid;
  grid-template-columns: minmax(0, 1fr) 36px;
  gap: 7px;
}

select {
  min-width: 0;
  height: 36px;
  padding: 0 10px;
  color: #cbd5e1;
  background: #0f172a;
  border: 1px solid #475569;
  border-radius: 5px;
  outline: none;
}

button {
  width: 36px;
  height: 36px;
  margin: 0;
  padding: 0;
  line-height: normal;
  color: #cbd5e1;
  background: #334155;
  border: 1px solid #475569;
  border-radius: 5px;
  font-size: 16px;
  cursor: pointer;
}

button:disabled,
select:disabled {
  cursor: not-allowed;
  opacity: 0.55;
}

.connect {
  width: 100%;
  margin-top: 14px;
  color: #fff;
  background: #2563eb;
  border-color: #3b82f6;
}

.connect:not(:disabled):hover {
  background: #1d4ed8;
}

.connect.disconnect {
  color: #fecaca;
  background: rgba(239, 68, 68, 0.14);
  border-color: rgba(248, 113, 113, 0.7);
}

.connect.disconnect:not(:disabled):hover {
  background: rgba(239, 68, 68, 0.24);
}

.refresh-button i {
  display: inline-block;
  font-size: 18px;
}

.refresh-button .spinning {
  animation: spin 0.8s linear infinite;
}

@keyframes spin {
  to { transform: rotate(360deg); }
}

@media (max-width: 720px) {
  .device-grid { grid-template-columns: 1fr; }
}
</style>
