<template>
  <article class="metric-card" :style="{ '--accent': accent }">
    <span class="accent-bar"></span>
    <div class="main-row">
      <span class="label">{{ label }}</span>
      <strong class="value">{{ value }}</strong>
      <span class="unit">{{ unit }}</span>
    </div>
    <div class="sub-row">
      <span>{{ subLabel }}</span>
      <strong>{{ subValue }}</strong>
      <span>{{ subUnit }}</span>
    </div>
  </article>
</template>

<script setup>
defineProps({
  label: { type: String, default: '' },
  value: { type: String, default: '0.00' },
  unit: { type: String, default: '' },
  subLabel: { type: String, default: '' },
  subValue: { type: String, default: '0.00' },
  subUnit: { type: String, default: '' },
  accent: { type: String, default: '#3b82f6' }
});
</script>

<style scoped>
.metric-card {
  position: relative;
  min-width: 0;
  padding: 14px 16px 12px 30px;
  overflow: hidden;
  background: #273549;
  border: 1px solid #475569;
  border-radius: 8px;
  transition: border-color 160ms ease, transform 160ms ease;
}
.metric-card:hover { border-color: var(--accent); transform: translateY(-1px); }
.accent-bar { position: absolute; left: 10px; top: 12px; bottom: 12px; width: 4px; background: var(--accent); border-radius: 2px; }
.main-row { display: grid; grid-template-columns: auto minmax(70px, 1fr) auto; align-items: baseline; gap: 8px; }
.label { color: #94a3b8; font-size: 15px; font-weight: 600; white-space: nowrap; }
.value { min-width: 0; color: #f8fafc; font: 800 28px/1.1 Consolas, monospace; text-align: center; overflow: hidden; }
.unit { color: #64748b; font-size: 14px; white-space: nowrap; }
.sub-row { display: flex; justify-content: flex-end; align-items: center; gap: 8px; margin-top: 9px; padding-top: 8px; border-top: 1px solid rgba(71, 85, 105, .55); color: #64748b; font-size: 16px; }
.sub-row strong { color: #94a3b8; font-family: Consolas, monospace; }
</style>
