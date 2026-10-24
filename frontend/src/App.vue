<template>
  <router-view />
</template>

<script setup>
</script>

<style>
* {
  margin: 0;
  padding: 0;
  box-sizing: border-box;
  user-select: none;
  /* -webkit-user-select: none; */
}

html, body {
  width: 100%;
  height: 100%;
  overflow: hidden;
  background: #d7e0ea;
  font-family: 'Inter', -apple-system, BlinkMacSystemFont, 'Segoe UI', system-ui, sans-serif;
  -webkit-font-smoothing: antialiased;
  -moz-osx-font-smoothing: grayscale;
  -webkit-text-size-adjust: 100%;
}

#app {
  width: 100%;
  max-width: none;
  height: 100%;
  margin: 0;
  overflow: hidden;
  text-align: initial;
}
</style>
