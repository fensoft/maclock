<script setup>
import MacButton from "./MacButton.vue";
defineProps({ open: Boolean, file: Object, busy: String, t: Function });
defineEmits(["cancel", "confirm"]);
</script>

<template>
  <Transition name="dialog"><div v-if="open" class="dialog-shade" @click.self="$emit('cancel')"><section class="classic-confirm" role="alertdialog" aria-modal="true" aria-labelledby="restore-backup-title" aria-describedby="restore-backup-message"><div class="confirm-icon" aria-hidden="true">!</div><div><h2 id="restore-backup-title">{{ t('restoreConfirmTitle') }}</h2><p id="restore-backup-message">{{ t('restoreConfirmMessage', { name: file?.name || '' }) }}</p><p>{{ t('restoreConfirmNetwork') }}</p></div><div class="confirm-actions"><MacButton secondary :disabled="!!busy" @click="$emit('cancel')">{{ t('cancel') }}</MacButton><MacButton danger default-action :disabled="!!busy" @click="$emit('confirm')">{{ busy === 'configuration-import' ? t('restoringBackup') : t('restoreBackup') }}</MacButton></div></section></div></Transition>
</template>
