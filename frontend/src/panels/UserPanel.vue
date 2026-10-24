<template>
  <div class="profile">
    <img src="../res/user.png" alt="用户头像">
    <div><h3>HEPS</h3><p>设备操作员</p></div>
  </div>
  <dl><div><dt>账户类型</dt><dd>本地账户</dd></div><div><dt>所属项目</dt><dd>MINIMTS</dd></div><div><dt>登录状态</dt><dd class="online">在线</dd></div><div><dt>最后登录</dt><dd>今天 09:30</dd></div></dl>
  <button class="logout" type="button"><i class="ri-logout-box-r-line"></i> 退出登录</button>
</template>

<style scoped>
.profile { display: flex; align-items: center; gap: 14px; padding-bottom: 18px; border-bottom: 1px solid #475569; }.profile img { width: 64px; height: 64px; border-radius: 50%; object-fit: cover; box-shadow: 0 0 0 3px rgba(59,130,246,.3); }.profile h3 { margin: 0; color: #f8fafc; font-size: 20px; }.profile p { margin: 5px 0 0; color: #94a3b8; font-size: 12px; }dl { margin: 16px 0; }dl div { display: flex; justify-content: space-between; padding: 10px 0; border-bottom: 1px solid #334155; }dt { color: #64748b; font-size: 12px; }dd { margin: 0; color: #cbd5e1; font-size: 12px; }.online { color: #34d399; }.logout { width: 100%; height: 38px; margin: 0; color: #fca5a5; background: rgba(239,68,68,.1); border: 1px solid rgba(239,68,68,.35); border-radius: 6px; }
</style>
