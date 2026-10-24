<template>
  <div class="message-panel">
    <header><strong>系统消息</strong><span>2 条</span></header>
    <div class="message warning"><i class="ri-error-warning-line"></i><div><strong>设备尚未连接</strong><p>请选择控制器串口并建立连接。</p><time>刚刚</time></div></div>
    <div class="message info"><i class="ri-information-line"></i><div><strong>项目参数待确认</strong><p>开始试验前请检查样品尺寸与速度。</p><time>10:24</time></div></div>
    <footer><button type="button">清除全部</button></footer>
  </div>
</template>

<style scoped>
.message-panel { width: 350px; overflow: hidden; color: #cbd5e1; background: #1e293b; border: 1px solid #475569; border-radius: 8px; box-shadow: 0 14px 35px rgba(0,0,0,.35); }
header { height: 48px; padding: 0 16px; display: flex; align-items: center; justify-content: space-between; border-bottom: 1px solid #334155; }
header strong { color: #f8fafc; font-size: 14px; } header span { color: #64748b; font-size: 12px; }
.message { display: flex; gap: 10px; margin: 12px; padding: 12px; background: #273549; border-left: 3px solid; border-radius: 5px; }
.message > i { margin-top: 2px; font-size: 18px; }.warning { border-color: #f59e0b; }.warning > i { color: #f59e0b; }.info { border-color: #3b82f6; }.info > i { color: #3b82f6; }
.message strong { color: #f1f5f9; font-size: 13px; }.message p { margin: 5px 0; color: #94a3b8; font-size: 12px; line-height: 1.5; }.message time { color: #64748b; font-size: 10px; }
footer { padding: 0 12px 12px; text-align: right; }button { width: auto; height: 28px; margin: 0; color: #94a3b8; background: transparent; border: 0; font-size: 12px; }
</style>
