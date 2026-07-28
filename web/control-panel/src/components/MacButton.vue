<script setup>
import { ref } from "vue";

defineProps({
  type: { type: String, default: "button" },
  secondary: Boolean,
  danger: Boolean,
  defaultAction: Boolean,
  disabled: Boolean,
});

defineEmits(["click"]);

const button = ref(null);

defineExpose({
  focus: () => button.value?.focus(),
});
</script>

<template>
  <span :class="{ 'mac-button-default': defaultAction }">
    <button
      ref="button"
      class="mac-button"
      :class="{ secondary, danger }"
      :type="type"
      :disabled="disabled"
      @click="$emit('click', $event)"
    >
      <slot />
    </button>
  </span>
</template>
