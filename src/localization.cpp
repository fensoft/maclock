#include "localization.h"

#include <string.h>

namespace
{
struct Translation
{
    const char *english;
    const char *french;
    const char *spanish;
    const char *german;
    const char *italian;
};

static UiLanguage g_language = UI_LANGUAGE_ENGLISH;

static const Translation g_translations[] = {
    {"Boot Options", "Options de démarrage", "Opciones de inicio", "Startoptionen", "Opzioni di avvio"},
    {"Start", "Démarrer", "Inicio", "Start", "Avvio"},
    {"Stop", "Arrêter", "Detener", "Stopp", "Ferma"},
    {"Preferences", "Préférences", "Preferencias", "Einstellungen", "Preferenze"},
    {"Language", "Langue", "Idioma", "Sprache", "Lingua"},
    {"Night Schedule", "Horaire nuit", "Horario nocturno", "Nachtzeit", "Orario notturno"},
    {"Night Screen", "Écran de nuit", "Pantalla nocturna", "Nachtbildschirm", "Schermo notturno"},
    {"Chime", "Carillon", "Campanada", "Glockenspiel", "Rintocco"},
    {"Chime Sound", "Son du carillon", "Sonido campanada", "Glockenton", "Suono rintocco"},
    {"Chime Volume", "Volume carillon", "Volumen campanada", "Glockenlautstärke", "Volume rintocco"},
    {"Quiet Hours", "Heures calmes", "Horas silenciosas", "Ruhezeiten", "Ore silenziose"},
    {"Tools", "Outils", "Herramientas", "Werkzeuge", "Strumenti"},
    {"Brightness", "Luminosité", "Brillo", "Helligkeit", "Luminosità"},
    {"Latest", "Dernière", "Último", "Zuletzt", "Ultima"},
    {"Lowest", "Minimum", "Mínimo", "Minimum", "Minima"},
    {"Highest", "Maximum", "Máximo", "Maximum", "Massima"},
    {"Default boot mode", "Mode au démarrage", "Modo de inicio", "Standard-Startmodus", "Modalità di avvio"},
    {"One time", "Une fois", "Una vez", "Einmal", "Una volta"},
    {"Remember", "Mémoriser", "Recordar", "Speichern", "Ricorda"},
    {"Disabled", "Désactivé", "Desactivado", "Deaktiviert", "Disattivato"},
    {"Enabled", "Activé", "Activado", "Aktiviert", "Attivato"},
    {"Dim from", "Atténuer dès", "Atenuar desde", "Dimmen ab", "Attenua dalle"},
    {"Normal at", "Normal à", "Normal a las", "Normal ab", "Normale alle"},
    {"Dim only", "Atténuer", "Solo atenuar", "Nur dimmen", "Solo attenua"},
    {"Screen off", "Écran éteint", "Pantalla apagada", "Bildschirm aus", "Schermo spento"},
    {"Screen off at", "Éteindre à", "Apagar a las", "Ausschalten um", "Spegni alle"},
    {"Off", "Arrêt", "Apagado", "Aus", "Spento"},
    {"On", "Marche", "Encendido", "Ein", "Acceso"},
    {"Hourly", "Toutes les heures", "Cada hora", "Stündlich", "Ogni ora"},
    {"Quarter hour", "Chaque quart d'heure", "Cada cuarto de hora", "Viertelstündlich", "Ogni quarto d'ora"},
    {"Quiet from", "Silence dès", "Silencio desde", "Ruhe ab", "Silenzio dalle"},
    {"Quiet ends", "Fin du silence", "Fin del silencio", "Ruhe endet", "Fine silenzio"},
    {"Setup Wi-Fi", "Configurer Wi-Fi", "Configurar Wi-Fi", "Wi-Fi einrichten", "Configura Wi-Fi"},
    {"Clock", "Horloge", "Reloj", "Uhr", "Orologio"},
    {"Emulator", "Émulateur", "Emulador", "Emulator", "Emulatore"},
    {"Diagnostics", "Diagnostic", "Diagnóstico", "Diagnose", "Diagnostica"},
    {"RTC: checking...", "RTC : vérification...", "RTC: comprobando...", "RTC: Prüfung...", "RTC: verifica..."},
    {"Press Clock for screen calibration", "Appuyez sur Horloge pour calibrer", "Pulse Reloj para calibrar", "Uhr zur Kalibrierung drücken", "Premi Orologio per calibrare"},
    {"Previous", "Précédent", "Anterior", "Zurück", "Precedente"},
    {"Exit", "Quitter", "Salir", "Beenden", "Esci"},
    {"Next", "Suivant", "Siguiente", "Weiter", "Successivo"},
    {"Back", "Retour", "Atrás", "Zurück", "Indietro"},
    {"Save", "Enregistrer", "Guardar", "Speichern", "Salva"},
    {"Cancel", "Annuler", "Cancelar", "Abbrechen", "Annulla"},
    {"Hardware Diagnostics", "Diagnostic matériel", "Diagnóstico hardware", "Hardwarediagnose", "Diagnostica hardware"},
    {"Checking hardware...", "Vérification du matériel...", "Comprobando hardware...", "Hardwareprüfung...", "Verifica hardware..."},
    {"RTC: not detected", "RTC : non détectée", "RTC: no detectado", "RTC: nicht erkannt", "RTC: non rilevato"},
    {"RTC: DS1307 stopped - check battery", "RTC : DS1307 arrêtée - vérifiez la pile", "RTC: DS1307 detenido - revise la pila", "RTC: DS1307 gestoppt - Batterie prüfen", "RTC: DS1307 fermo - controlla la batteria"},
    {"RTC: DS3231 lost power - check battery", "RTC : alimentation DS3231 perdue - vérifiez la pile", "RTC: DS3231 sin energía - revise la pila", "RTC: DS3231 Stromausfall - Batterie prüfen", "RTC: DS3231 senza alimentazione - controlla la batteria"},
    {"RTC: invalid date", "RTC : date invalide", "RTC: fecha no válida", "RTC: ungültiges Datum", "RTC: data non valida"},
    {"RTC: date not set (%04d)", "RTC : date non réglée (%04d)", "RTC: fecha no ajustada (%04d)", "RTC: Datum nicht gesetzt (%04d)", "RTC: data non impostata (%04d)"},
    {"RTC: %s OK", "RTC : %s OK", "RTC: %s OK", "RTC: %s OK", "RTC: %s OK"},
    {"Floppy", "Disquette", "Disquete", "Diskette", "Floppy"},
    {"Encoder", "Encodeur", "Codificador", "Drehgeber", "Encoder"},
    {"Touch", "Tactile", "Táctil", "Touch", "Tocco"},
    {"Charging", "Recharge", "Carga", "Laden", "Ricarica"},
    {"Pressed", "Appuyé", "Pulsado", "Gedrückt", "Premuto"},
    {"Released", "Relâché", "Suelto", "Losgelassen", "Rilasciato"},
    {"Inserted", "Insérée", "Insertado", "Eingelegt", "Inserito"},
    {"Empty", "Vide", "Vacío", "Leer", "Vuoto"},
    {"Yes", "Oui", "Sí", "Ja", "Sì"},
    {"No", "Non", "No", "Nein", "No"},
    {"None", "Aucun", "Ninguno", "Keine", "Nessuno"},
    {"Setup portal", "Portail de configuration", "Portal de configuración", "Einrichtungsportal", "Portale di configurazione"},
    {"Not configured", "Non configuré", "Sin configurar", "Nicht konfiguriert", "Non configurato"},
    {"Online", "En ligne", "En línea", "Online", "Online"},
    {"Offline", "Hors ligne", "Sin conexión", "Offline", "Offline"},
    {"Wi-Fi Setup", "Configuration Wi-Fi", "Configuración Wi-Fi", "Wi-Fi-Einrichtung", "Configurazione Wi-Fi"},
    {"Connect to: Maclock Setup\nThen open: 192.168.4.1", "Connectez-vous à : Maclock Setup\nPuis ouvrez : 192.168.4.1", "Conéctese a: Maclock Setup\nLuego abra: 192.168.4.1", "Verbinden mit: Maclock Setup\nDann öffnen: 192.168.4.1", "Connettiti a: Maclock Setup\nPoi apri: 192.168.4.1"},
    {"1. Connect to Wi-Fi:\nMaclock Setup\n\n2. Open 192.168.4.1\n\n%s", "1. Connectez-vous au Wi-Fi :\nMaclock Setup\n\n2. Ouvrez 192.168.4.1\n\n%s", "1. Conéctese al Wi-Fi:\nMaclock Setup\n\n2. Abra 192.168.4.1\n\n%s", "1. Mit Wi-Fi verbinden:\nMaclock Setup\n\n2. 192.168.4.1 öffnen\n\n%s", "1. Connettiti al Wi-Fi:\nMaclock Setup\n\n2. Apri 192.168.4.1\n\n%s"},
    {"Wi-Fi disabled\nClock remains fully offline", "Wi-Fi désactivé\nL'horloge reste hors ligne", "Wi-Fi desactivado\nEl reloj queda sin conexión", "Wi-Fi deaktiviert\nDie Uhr bleibt offline", "Wi-Fi disattivato\nL'orologio resta offline"},
    {"Setup required\nChoose Setup Wi-Fi below", "Configuration requise\nChoisissez Configurer Wi-Fi", "Configuración necesaria\nElija Configurar Wi-Fi", "Einrichtung erforderlich\nWi-Fi einrichten wählen", "Configurazione necessaria\nScegli Configura Wi-Fi"},
    {"Online: %s\n%s", "En ligne : %s\n%s", "En línea: %s\n%s", "Online: %s\n%s", "Online: %s\n%s"},
    {"Touch the crosshair", "Touchez la croix", "Toque la cruz", "Kreuz berühren", "Tocca la croce"},
    {"Calibration failed - try again", "Échec du calibrage - réessayez", "Calibración fallida - reintente", "Kalibrierung fehlgeschlagen", "Calibrazione fallita - riprova"},
    {"In", "Int", "Int", "Innen", "Int"},
    {"Out", "Ext", "Ext", "Außen", "Est"},
    {"Today", "Jour", "Hoy", "Heute", "Oggi"},
    {"Alarm / Timer", "Alarme / Minuteur", "Alarma / Temporizador", "Alarm / Timer", "Sveglia / Timer"},
    {"Alarm", "Alarme", "Alarma", "Alarm", "Sveglia"},
    {"Alarm 1", "Alarme 1", "Alarma 1", "Alarm 1", "Sveglia 1"},
    {"Alarm 2", "Alarme 2", "Alarma 2", "Alarm 2", "Sveglia 2"},
    {"Alarm 3", "Alarme 3", "Alarma 3", "Alarm 3", "Sveglia 3"},
    {"Alarms", "Alarmes", "Alarmas", "Alarme", "Sveglie"},
    {"Timer", "Minuteur", "Temporizador", "Timer", "Timer"},
    {"Time", "Heure", "Hora", "Zeit", "Ora"},
    {"Days", "Jours", "Días", "Tage", "Giorni"},
    {"Sound", "Son", "Sonido", "Ton", "Suono"},
    {"Volume", "Volume", "Volumen", "Lautstärke", "Volume"},
    {"Actions", "Actions", "Acciones", "Aktionen", "Azioni"},
    {"Hour -", "Heure -", "Hora -", "Stunde -", "Ora -"},
    {"Hour +", "Heure +", "Hora +", "Stunde +", "Ora +"},
    {"Minute -", "Minute -", "Minuto -", "Minute -", "Minuto -"},
    {"Minute +", "Minute +", "Minuto +", "Minute +", "Minuto +"},
    {"Mon", "Lun", "Lun", "Mo", "Lun"},
    {"Tue", "Mar", "Mar", "Di", "Mar"},
    {"Wed", "Mer", "Mié", "Mi", "Mer"},
    {"Thu", "Jeu", "Jue", "Do", "Gio"},
    {"Fri", "Ven", "Vie", "Fr", "Ven"},
    {"Sat", "Sam", "Sáb", "Sa", "Sab"},
    {"Sun", "Dim", "Dom", "So", "Dom"},
    {"Snooze 9 min", "Rappel 9 min", "Posponer 9 min", "9 Min schlummern", "Rinvia 9 min"},
    {"Dismiss", "Fermer", "Descartar", "Schließen", "Chiudi"},
    {"Adjust duration", "Régler la durée", "Ajustar duración", "Dauer einstellen", "Regola durata"},
    {"Running in background", "Actif en arrière-plan", "Activo en segundo plano", "Läuft im Hintergrund", "Attivo in background"},
    {"Timer Complete", "Minuteur terminé", "Temporizador terminado", "Timer beendet", "Timer completato"},
    {"Press Clock or Alarm to dismiss", "Appuyez sur Horloge ou Alarme", "Pulse Reloj o Alarma", "Uhr oder Alarm drücken", "Premi Orologio o Sveglia"},
    {"Set Date/Time", "Régler date/heure", "Ajustar fecha/hora", "Datum/Uhrzeit stellen", "Imposta data/ora"},
    {"Play", "Lire", "Reproducir", "Abspielen", "Riproduci"},
    {"No MP3 files in LittleFS", "Aucun MP3 dans LittleFS", "Sin MP3 en LittleFS", "Keine MP3 in LittleFS", "Nessun MP3 in LittleFS"},
};
}

void localization_set_language(UiLanguage language)
{
    g_language = language < UI_LANGUAGE_COUNT
                     ? language
                     : UI_LANGUAGE_ENGLISH;
}

UiLanguage localization_get_language()
{
    return g_language;
}

const char *localization_language_name(UiLanguage language)
{
    static const char *names[UI_LANGUAGE_COUNT] = {
        "English", "French", "Spanish", "Deutsch", "Italian"};
    return language < UI_LANGUAGE_COUNT
               ? names[language]
               : names[UI_LANGUAGE_ENGLISH];
}

const char *tr(const char *english)
{
    if (!english || g_language == UI_LANGUAGE_ENGLISH)
        return english ? english : "";

    for (const Translation &translation : g_translations)
    {
        if (strcmp(translation.english, english) != 0)
            continue;
        switch (g_language)
        {
        case UI_LANGUAGE_FRENCH:
            return translation.french;
        case UI_LANGUAGE_SPANISH:
            return translation.spanish;
        case UI_LANGUAGE_GERMAN:
            return translation.german;
        case UI_LANGUAGE_ITALIAN:
            return translation.italian;
        default:
            return translation.english;
        }
    }
    return english;
}
