<script setup>
defineProps({ activeApp: String, t: Function });
defineEmits(["home", "open"]);
</script>

<template>
  <nav class="menu-bar" :class="{ 'menu-bar--face-editor': activeApp === 'faceEditor' || activeApp === 'loadingEditor' }" :aria-label="t('menuSections')">
    <button class="apple" type="button" :aria-label="t('home')" @click="$emit('home')">
      <svg viewBox="0 0 24 24" aria-hidden="true"><path d="M17.05 20.28c-.98.95-2.05.8-3.08.35-1.09-.46-2.09-.48-3.24 0-1.44.62-2.2.44-3.06-.35C2.79 15.25 3.51 7.59 9.05 7.31c1.35-.07 2.29.74 3.08.79 1.18-.24 2.31-.93 3.57-.84 1.51.12 2.65.72 3.4 1.8-3.12 1.87-2.38 5.98.48 7.13-.57 1.5-1.31 2.99-2.53 4.09M12.03 7.25C11.88 5.02 13.69 3.18 15.77 3c.29 2.58-2.34 4.5-3.74 4.25" /></svg>
    </button>
    <div v-if="activeApp === 'faceEditor' || activeApp === 'loadingEditor'" id="face-editor-menu-host"></div>
    <template v-else>
      <button type="button" @click="$emit('open', 'appearance')">{{ t('file') }}</button>
      <button type="button" @click="$emit('open', 'alarms')">{{ t('edit') }}</button>
      <button type="button" @click="$emit('open', 'screensaver')">{{ t('view') }}</button>
      <button type="button" @click="$emit('open', 'sounds')">{{ t('special') }}</button>
      <span>{{ t('control') }}</span>
    </template>
  </nav>
</template>
