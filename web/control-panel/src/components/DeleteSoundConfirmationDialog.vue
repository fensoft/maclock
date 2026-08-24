<script setup>
import MacButton from "./MacButton.vue";
defineProps({ sound: Object, busy: String, t: Function });
defineEmits(["cancel", "confirm"]);
</script>

<template>
  <Transition name="dialog"><div v-if="sound" class="dialog-shade" @click.self="$emit('cancel')"><section class="classic-confirm" role="alertdialog" aria-modal="true" aria-labelledby="delete-sound-title" aria-describedby="delete-sound-message"><div class="confirm-icon" aria-hidden="true">?</div><div><h2 id="delete-sound-title">{{ t('removeSoundTitle') }}</h2><p id="delete-sound-message">{{ t('removeSoundMessage', { name: sound.name }) }}</p></div><div class="confirm-actions"><MacButton secondary :disabled="!!busy" @click="$emit('cancel')">{{ t('cancel') }}</MacButton><MacButton danger default-action :disabled="!!busy" @click="$emit('confirm')">{{ t('remove') }}</MacButton></div></section></div></Transition>
</template>
