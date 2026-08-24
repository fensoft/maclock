<script setup>
import MacWindow from "./MacWindow.vue";
defineProps({ appId: String, title: String, closeLabel: String, wide: Boolean });
defineEmits(["close"]);
defineExpose({ focusAndReveal: () => windowRef.value?.focusAndReveal() });
import { ref } from "vue";
const windowRef = ref(null);
</script>

<template>
  <MacWindow ref="windowRef" :id="appId" :title="title" :wide="wide" closable :close-label="closeLabel" @close="$emit('close')"><slot /></MacWindow>
</template>
