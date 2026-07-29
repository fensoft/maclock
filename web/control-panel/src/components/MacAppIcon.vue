<script setup>
defineProps({
  appId: { type: String, required: true },
  icon: { type: String, required: true },
  title: { type: String, required: true },
  openLabel: { type: String, required: true },
  selected: Boolean,
});

const emit = defineEmits(["select", "open"]);

function handlePointerUp(event) {
  if (event.pointerType === "touch") {
    emit("open");
    return;
  }
  emit("select");
}
</script>

<template>
  <button
    class="desktop-app"
    :class="{ 'desktop-app--selected': selected }"
    type="button"
    :aria-label="openLabel"
    :aria-pressed="selected"
    :data-app="appId"
    @pointerup="handlePointerUp"
    @dblclick.prevent="$emit('open')"
    @keydown.enter.prevent="$emit('open')"
    @keydown.space.prevent="$emit('open')"
  >
    <span class="desktop-app__icon" aria-hidden="true">
      <svg viewBox="0 0 16 16" shape-rendering="crispEdges">
        <g v-if="icon === 'appearance'">
          <path d="M1 2h14v10H1z" fill="#fff" stroke="#000" />
          <path d="M3 4h10v6H3zM7 12h2v2H7zM4 14h8v1H4z" />
          <path d="M4 5h4v1H4zM4 7h7v1H4z" fill="#fff" />
        </g>
        <g v-else-if="icon === 'location'">
          <path d="M6 1h4v1h2v1h1v5h-1v2h-1v2h-1v2H6v-2H5v-2H4V8H3V3h1V2h2z" />
          <path d="M6 3h4v1h1v4h-1v2H9v2H7v-2H6V8H5V4h1z" fill="#fff" />
          <path d="M7 5h2v2H7z" />
        </g>
        <g v-else-if="icon === 'mqtt'">
          <path d="M7 9h2v5H7zM5 14h6v1H5zM6 7h4v3H6z" />
          <path d="M4 5h2v1H5v3H4zm6 0h2v4h-1V6h-1z" />
          <path d="M2 3h2v1H3v6H2zm10 0h2v7h-1V4h-1z" />
          <path d="M1 1h2v1H2v10H1zm12 0h2v11h-1V2h-1z" />
        </g>
        <g v-else-if="icon === 'screensaver'">
          <rect x="1" y="2" width="14" height="11" fill="#fff" stroke="#000" />
          <rect x="2" y="3" width="12" height="9" />
          <path d="M4 5h1v1H4zm6-1h1v1h-1zM7 8h1v1H7zm5 2h1v1h-1zM3 10h1v1H3z" fill="#fff" />
          <rect x="6" y="13" width="4" height="2" />
        </g>
        <g v-else-if="icon === 'timer'">
          <path d="M6 1h4v2H9v1h3v1h1v2h1v5h-1v2h-2v1H5v-1H3v-2H2V7h1V5h2V4h2V3H6z" />
          <path d="M5 6h6v1h1v5h-1v1H5v-1H4V7h1z" fill="#fff" />
          <path d="M7 7h2v4h2v1H7z" />
        </g>
        <g v-else-if="icon === 'alarm'">
          <path d="M3 1h3v1H4v1H2V2h1zm7 0h3v1h1v1h-2V2h-2zM5 3h6v1h2v2h1v6h-2v2h-1v1H5v-1H4v-2H2V6h1V4h2z" />
          <path d="M5 5h6v1h1v6h-1v1H5v-1H4V6h1z" fill="#fff" />
          <path d="M7 6h2v4h2v1H7zM3 13h2v2H2v-1h1zm8 0h2v1h1v1h-3z" />
        </g>
        <g v-else-if="icon === 'night'">
          <path d="M6 1h5v1H9v1H7v2H6v6h1v2h2v1h2v1H6v-1H4v-1H3v-2H2V6h1V4h1V2h2z" />
          <path d="M11 4h1v2h2v1h-2v2h-1V7H9V6h2zm2 7h1v1h1v1h-1v1h-1v-1h-1v-1h1z" />
        </g>
        <g v-else-if="icon === 'chime'">
          <path d="M7 1h2v2h2v1h1v2h1v5h2v2H1v-2h2V6h1V4h1V3h2zM6 14h4v1H6z" />
          <path d="M6 4h4v1h1v6H5V5h1z" fill="#fff" />
        </g>
        <g v-else-if="icon === 'backup'">
          <path d="M2 1h12v14H2z" fill="#fff" stroke="#000" />
          <path d="M4 2h7v5H4z" fill="#9cf" stroke="#000" />
          <path d="M5 3h4v3H5z" fill="#fff" />
          <path d="M4 9h8v5H4z" fill="#fd9" stroke="#000" />
          <path d="M6 10h4v3H6z" fill="#fff" stroke="#000" />
          <path d="M11 2h1v4h-1z" />
        </g>
        <g v-else-if="icon === 'update'">
          <path d="M7 1h2v2h2v1h1v1h1v2h2l-3 4-3-4h2V6h-1V5H9V4H7V3H5V2h2z" />
          <path d="M3 5h2v1H4v1H3v5h1v1h8v-1h1v-1h2v2h-1v1h-2v1H4v-1H2v-2H1V7h1V6h1z" />
        </g>
        <g v-else>
          <path d="M1 6h4l4-4v12l-4-4H1z" />
          <path d="M11 5h1v1h1v4h-1v1h-1v-2h1V7h-1zm2-2h1v1h1v8h-1v1h-1v-2h1V5h-1z" />
        </g>
      </svg>
    </span>
    <span class="desktop-app__label">{{ title }}</span>
  </button>
</template>
