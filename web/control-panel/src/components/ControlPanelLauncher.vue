<script setup>
import { ref } from "vue";
import MacAppIcon from "./MacAppIcon.vue";
defineProps({ apps: Array, selectedApp: String, t: Function });
defineEmits(["select", "open"]);
const launcher = ref(null);
defineExpose({ focus: (options) => launcher.value?.focus(options), scrollIntoView: (options) => launcher.value?.scrollIntoView(options) });
</script>

<template>
  <section ref="launcher" class="app-launcher" tabindex="-1" :aria-label="t('launcherTitle')">
    <div class="app-grid"><MacAppIcon v-for="app in apps" :key="app.id" :app-id="app.id" :icon="app.icon" :title="t(app.titleKey)" :open-label="t('openApp', { title: t(app.titleKey) })" :selected="selectedApp === app.id" @select="$emit('select', app.id)" @open="$emit('open', app.id)" /></div>
  </section>
</template>
