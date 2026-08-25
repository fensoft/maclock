<script setup>
import MacButton from "./MacButton.vue";
defineProps({ notice: Object, t: Function });
defineEmits(["dismiss"]);
</script>

<template>
  <Transition name="alert"><div v-if="notice" class="classic-alert" :class="{ error: notice.kind === 'error' }" role="status"><div class="alert-icon" aria-hidden="true">{{ notice.kind === 'error' ? '!' : '✓' }}</div><p>{{ notice.message }}</p><MacButton default-action @click="$emit('dismiss')">{{ t('ok') }}</MacButton></div></Transition>
</template>
