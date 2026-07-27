<script setup>
defineProps({
  title: { type: String, required: true },
  wide: Boolean,
  compact: Boolean,
});
</script>

<template>
  <section
    class="mac-window"
    :class="{ 'mac-window--wide': wide, 'mac-window--compact': compact }"
  >
    <header class="mac-window__bar">
      <span class="mac-window__box" aria-hidden="true"></span>
      <h2>{{ title }}</h2>
      <span class="mac-window__box mac-window__box--zoom" aria-hidden="true"></span>
    </header>
    <div class="mac-window__body">
      <slot />
    </div>
  </section>
</template>
