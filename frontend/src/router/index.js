import { createRouter, createWebHashHistory } from 'vue-router'
import MINIMTS from '../components/MINIMTS.vue'
import Camera from '../components/Camera.vue'
import Calibration from '../components/Calibration.vue'

const routes = [
  {
    path: '/',
    name: 'MINIMTS',
    component: MINIMTS
  },
  {
    path: '/camera',
    name: 'Camera',
    component: Camera
  },
  {
    path: '/calibration',
    name: 'Calibration',
    component: Calibration
  },
]

const router = createRouter({
  history: createWebHashHistory(),
  routes
})

export default router