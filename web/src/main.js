import { createApp } from 'vue'
import App from './App.vue'

async function boot() {
  await window.__f1simReady
  createApp(App).mount('#app')
}

boot()

