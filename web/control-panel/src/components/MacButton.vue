<script setup>
defineProps({
  type: { type: String, default: "button" },
  secondary: Boolean,
  danger: Boolean,
  defaultAction: Boolean,
  disabled: Boolean,
});

defineEmits(["click"]);
</script>

<template>
  <span :class="{ 'mac-button-default': defaultAction }">
    <button
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
