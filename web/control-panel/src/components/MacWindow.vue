<script setup>
import { ref } from "vue";

defineProps({
  title: { type: String, required: true },
  wide: Boolean,
  compact: Boolean,
  closable: Boolean,
  closeLabel: { type: String, default: "Close" },
});

defineEmits(["close"]);

const root = ref(null);
const heading = ref(null);

function focusAndReveal() {
  root.value?.scrollIntoView({
    behavior: window.matchMedia("(prefers-reduced-motion: reduce)").matches
      ? "auto"
      : "smooth",
    block: "start",
  });
  heading.value?.focus({ preventScroll: true });
}

defineExpose({ focusAndReveal });
</script>

<template>
  <section
    ref="root"
    class="mac-window"
    :class="{ 'mac-window--wide': wide, 'mac-window--compact': compact }"
  >
    <header class="mac-window__bar">
      <button
        v-if="closable"
        class="mac-window__box mac-window__close"
        type="button"
        :aria-label="closeLabel"
        @click="$emit('close')"
      ></button>
      <span v-else class="mac-window__box" aria-hidden="true"></span>
      <h2 ref="heading" tabindex="-1">{{ title }}</h2>
      <span class="mac-window__box mac-window__box--zoom" aria-hidden="true"></span>
    </header>
    <div class="mac-window__body">
      <slot />
    </div>
  </section>
</template>
