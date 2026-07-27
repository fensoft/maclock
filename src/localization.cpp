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
    {"Configuration", "Configuration", "Configuración", "Konfiguration", "Configurazione"},
    {"General", "Général", "General", "Allgemein", "Generale"},
    {"System", "Système", "Sistema", "System", "Sistema"},
    {"Sections", "Sections", "Secciones", "Bereiche", "Sezioni"},
    {"Start", "Démarrer", "Inicio", "Start", "Avvio"},
    {"Stop", "Arrêter", "Detener", "Stopp", "Ferma"},
    {"Preferences", "Préférences", "Preferencias", "Einstellungen", "Preferenze"},
    {"Language", "Langue", "Idioma", "Sprache", "Lingua"},
    {"Regional", "Région", "Regional", "Region", "Regione"},
    {"Date / Time", "Date / Heure", "Fecha / Hora", "Datum / Uhrzeit", "Data / Ora"},
    {"Set Date / Time", "Régler date / heure", "Ajustar fecha / hora", "Datum / Uhrzeit stellen", "Imposta data / ora"},
    {"Hour", "Heure", "Hora", "Stunde", "Ora"},
    {"Minute", "Minute", "Minuto", "Minute", "Minuto"},
    {"Second", "Seconde", "Segundo", "Sekunde", "Secondo"},
    {"Seconds", "Secondes", "Segundos", "Sekunden", "Secondi"},
    {"Day", "Jour", "Día", "Tag", "Giorno"},
    {"Month", "Mois", "Mes", "Monat", "Mese"},
    {"Year", "Année", "Año", "Jahr", "Anno"},
    {"Date format", "Format de date", "Formato de fecha", "Datumsformat", "Formato data"},
    {"Temperature unit", "Unité de température", "Unidad de temperatura", "Temperatureinheit", "Unità temperatura"},
    {"Time Format", "Format de l'heure", "Formato de hora", "Zeitformat", "Formato ora"},
    {"Display", "Affichage", "Pantalla", "Anzeige", "Schermo"},
    {"24-hour", "24 heures", "24 horas", "24 Stunden", "24 ore"},
    {"12-hour", "12 heures", "12 horas", "12 Stunden", "12 ore"},
    {"No leading zero", "Sans zéro initial", "Sin cero inicial", "Ohne führende Null", "Senza zero iniziale"},
    {"Leading zero", "Zéro initial", "Cero inicial", "Führende Null", "Zero iniziale"},
    {"Initial zero", "Zéro initial", "Cero inicial", "Führende Null", "Zero iniziale"},
    {"Hide seconds", "Masquer secondes", "Ocultar segundos", "Sekunden aus", "Nascondi secondi"},
    {"Show seconds", "Afficher secondes", "Mostrar segundos", "Sekunden an", "Mostra secondi"},
    {"Hide weekday", "Masquer le jour", "Ocultar día", "Wochentag aus", "Nascondi giorno"},
    {"Show weekday", "Afficher le jour", "Mostrar día", "Wochentag an", "Mostra giorno"},
    {"Clock Face", "Cadran", "Esfera", "Zifferblatt", "Quadrante"},
    {"Clock Theme", "Thème d'horloge", "Tema del reloj", "Uhrendesign", "Tema orologio"},
    {"Light", "Clair", "Claro", "Hell", "Chiaro"},
    {"Dark", "Sombre", "Oscuro", "Dunkel", "Scuro"},
    {"Dark mode", "Mode sombre", "Modo oscuro", "Dunkelmodus", "Modalità scura"},
    {"Macintosh", "Macintosh", "Macintosh", "Macintosh", "Macintosh"},
    {"Compact", "Compact", "Compacto", "Kompakt", "Compatto"},
    {"Analog", "Analogique", "Analógico", "Analog", "Analogico"},
    {"Flip", "À volets", "Flip", "Klappzahlen", "A palette"},
    {"Screensaver", "Économiseur", "Salvapantallas", "Bildschirmschoner", "Salvaschermo"},
    {"After Dark", "After Dark", "After Dark", "After Dark", "After Dark"},
    {"Start after", "Démarrer après", "Iniciar después", "Start nach", "Avvia dopo"},
    {"1 min", "1 min", "1 min", "1 Min.", "1 min"},
    {"5 min", "5 min", "5 min", "5 Min.", "5 min"},
    {"10 min", "10 min", "10 min", "10 Min.", "10 min"},
    {"30 min", "30 min", "30 min", "30 Min.", "30 min"},
    {"Night Schedule", "Horaire nuit", "Horario nocturno", "Nachtzeit", "Orario notturno"},
    {"Night Screen", "Écran de nuit", "Pantalla nocturna", "Nachtbildschirm", "Schermo notturno"},
    {"Chime", "Carillon", "Campanada", "Glockenspiel", "Rintocco"},
    {"Chime Sound", "Son du carillon", "Sonido campanada", "Glockenton", "Suono rintocco"},
    {"Chime Volume", "Volume carillon", "Volumen campanada", "Glockenlautstärke", "Volume rintocco"},
    {"Quiet Hours", "Heures calmes", "Horas silenciosas", "Ruhezeiten", "Ore silenziose"},
    {"Tools", "Outils", "Herramientas", "Werkzeuge", "Strumenti"},
    {"About", "À propos", "Acerca de", "Über", "Informazioni"},
    {"Author: fensoft", "Auteur : fensoft", "Autor: fensoft", "Autor: fensoft", "Autore: fensoft"},
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
    {"File", "Fichier", "Archivo", "Ablage", "Archivio"},
    {"Edit", "Édition", "Edición", "Bearbeiten", "Composizione"},
    {"View", "Présentation", "Visualización", "Darstellung", "Vista"},
    {"Special", "Spécial", "Especial", "Spezial", "Speciale"},
    {"Welcome to Macintosh.", "Bienvenue sur Macintosh.", "Bienvenido a Macintosh.", "Willkommen bei Macintosh.", "Benvenuto in Macintosh."},
    {"Emulator", "Émulateur", "Emulador", "Emulator", "Emulatore"},
    {"Diagnostics", "Diagnostic", "Diagnóstico", "Diagnose", "Diagnostica"},
    {"RTC: checking...", "RTC : vérification...", "RTC: comprobando...", "RTC: Prüfung...", "RTC: verifica..."},
    {"RTC not available", "RTC indisponible", "RTC no disponible", "RTC nicht verfügbar", "RTC non disponibile"},
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
    {"Connect to Maclock Setup", "Connectez-vous à Maclock Setup", "Conéctese a Maclock Setup", "Mit Maclock Setup verbinden", "Connettiti a Maclock Setup"},
    {"Scan to join\nMaclock Setup\n(iPhone / Android)\n\nOpen 192.168.4.1\n%s", "Scannez pour rejoindre\nMaclock Setup\n(iPhone / Android)\n\nOuvrez 192.168.4.1\n%s", "Escanee para unirse a\nMaclock Setup\n(iPhone / Android)\n\nAbra 192.168.4.1\n%s", "Scannen und verbinden\nMaclock Setup\n(iPhone / Android)\n\n192.168.4.1 öffnen\n%s", "Scansiona per connetterti\nMaclock Setup\n(iPhone / Android)\n\nApri 192.168.4.1\n%s"},
    {"Finding configured city...", "Recherche de la ville...", "Buscando la ciudad...", "Stadt wird gesucht...", "Ricerca della città..."},
    {"City lookup connection failed", "Connexion à la recherche de ville impossible", "Falló la conexión para buscar la ciudad", "Verbindung zur Stadtsuche fehlgeschlagen", "Connessione alla ricerca città non riuscita"},
    {"City service connection failed", "Connexion au service des villes impossible", "Falló la conexión al servicio de ciudades", "Verbindung zum Stadtdienst fehlgeschlagen", "Connessione al servizio città non riuscita"},
    {"City service error", "Erreur du service des villes", "Error del servicio de ciudades", "Fehler beim Stadtdienst", "Errore del servizio città"},
    {"City service returned no data", "Le service des villes n'a renvoyé aucune donnée", "El servicio de ciudades no devolvió datos", "Stadtdienst lieferte keine Daten", "Il servizio città non ha restituito dati"},
    {"City response could not be read", "Réponse de la ville illisible", "No se pudo leer la respuesta de la ciudad", "Stadtantwort konnte nicht gelesen werden", "Impossibile leggere la risposta della città"},
    {"City not found - check spelling", "Ville introuvable - vérifiez l'orthographe", "Ciudad no encontrada - revise la ortografía", "Stadt nicht gefunden - Schreibweise prüfen", "Città non trovata - controlla l'ortografia"},
    {"Updating online forecast...", "Mise à jour des prévisions...", "Actualizando el pronóstico...", "Online-Vorhersage wird aktualisiert...", "Aggiornamento delle previsioni..."},
    {"Synchronizing clock...", "Synchronisation de l'horloge...", "Sincronizando el reloj...", "Uhr wird synchronisiert...", "Sincronizzazione dell'orologio..."},
    {"Connecting to Wi-Fi...", "Connexion au Wi-Fi...", "Conectando al Wi-Fi...", "Verbindung mit Wi-Fi...", "Connessione al Wi-Fi..."},
    {"Wi-Fi is disabled", "Le Wi-Fi est désactivé", "El Wi-Fi está desactivado", "Wi-Fi ist deaktiviert", "Il Wi-Fi è disattivato"},
    {"Setup is required", "Configuration requise", "Se requiere configuración", "Einrichtung erforderlich", "Configurazione necessaria"},
    {"Offline - retrying soon", "Hors ligne - nouvel essai prochainement", "Sin conexión - nuevo intento pronto", "Offline - neuer Versuch folgt", "Offline - nuovo tentativo a breve"},
    {"Forecast unavailable - using sensor", "Prévisions indisponibles - capteur utilisé", "Pronóstico no disponible - usando sensor", "Vorhersage nicht verfügbar - Sensor aktiv", "Previsioni non disponibili - uso del sensore"},
    {"Online - NTP unavailable", "En ligne - NTP indisponible", "En línea - NTP no disponible", "Online - NTP nicht verfügbar", "Online - NTP non disponibile"},
    {"Online - forecast updated", "En ligne - prévisions mises à jour", "En línea - pronóstico actualizado", "Online - Vorhersage aktualisiert", "Online - previsioni aggiornate"},
    {"Online - cached forecast", "En ligne - prévisions en cache", "En línea - pronóstico en caché", "Online - Vorhersage aus Cache", "Online - previsioni dalla cache"},
    {"Online - using local sensor", "En ligne - capteur local utilisé", "En línea - usando sensor local", "Online - lokaler Sensor aktiv", "Online - uso del sensore locale"},
    {"Saved - exit setup to connect", "Enregistré - quittez pour vous connecter", "Guardado - salga para conectarse", "Gespeichert - zum Verbinden beenden", "Salvato - esci per connetterti"},
    {"Waiting to connect", "En attente de connexion", "Esperando conexión", "Warten auf Verbindung", "In attesa di connessione"},
    {"Wi-Fi worker could not start", "Impossible de démarrer le service Wi-Fi", "No se pudo iniciar el servicio Wi-Fi", "Wi-Fi-Dienst konnte nicht starten", "Impossibile avviare il servizio Wi-Fi"},
    {"Could not start setup network", "Impossible de démarrer le réseau de configuration", "No se pudo iniciar la red de configuración", "Einrichtungsnetz konnte nicht starten", "Impossibile avviare la rete di configurazione"},
    {"Wi-Fi disabled\nClock remains fully offline", "Wi-Fi désactivé\nL'horloge reste hors ligne", "Wi-Fi desactivado\nEl reloj queda sin conexión", "Wi-Fi deaktiviert\nDie Uhr bleibt offline", "Wi-Fi disattivato\nL'orologio resta offline"},
    {"Setup required\nChoose Setup Wi-Fi below", "Configuration requise\nChoisissez Configurer Wi-Fi", "Configuración necesaria\nElija Configurar Wi-Fi", "Einrichtung erforderlich\nWi-Fi einrichten wählen", "Configurazione necessaria\nScegli Configura Wi-Fi"},
    {"Online: %s\n%s", "En ligne : %s\n%s", "En línea: %s\n%s", "Online: %s\n%s", "Online: %s\n%s"},
    {"Detected networks", "Réseaux détectés", "Redes detectadas", "Gefundene Netzwerke", "Reti rilevate"},
    {"Secured", "Sécurisé", "Protegida", "Gesichert", "Protetta"},
    {"Open network", "Réseau ouvert", "Red abierta", "Offen", "Rete aperta"},
    {"No networks found. Enter a name below.", "Aucun réseau trouvé. Saisissez un nom ci-dessous.", "No se encontraron redes. Introduzca un nombre abajo.", "Keine Netzwerke gefunden. Namen unten eingeben.", "Nessuna rete trovata. Inserisci un nome sotto."},
    {"Network scan failed. Enter a name below.", "Échec de la recherche. Saisissez un nom ci-dessous.", "Falló la búsqueda. Introduzca un nombre abajo.", "Netzwerksuche fehlgeschlagen. Namen unten eingeben.", "Ricerca reti non riuscita. Inserisci un nome sotto."},
    {"Wi-Fi name", "Nom du Wi-Fi", "Nombre del Wi-Fi", "Wi-Fi-Name", "Nome Wi-Fi"},
    {"Password", "Mot de passe", "Contraseña", "Passwort", "Password"},
    {"Leave empty to keep the saved password, or for a new open network.", "Laissez vide pour conserver le mot de passe ou pour un nouveau réseau ouvert.", "Déjelo vacío para conservar la contraseña o para una nueva red abierta.", "Leer lassen, um das gespeicherte Passwort zu behalten oder für ein neues offenes Netz.", "Lascia vuoto per mantenere la password o per una nuova rete aperta."},
    {"City", "Ville", "Ciudad", "Stadt", "Città"},
    {"Used for timezone, DST, and weather.", "Utilisée pour le fuseau horaire, l'heure d'été et la météo.", "Se usa para la zona horaria, el horario de verano y el tiempo.", "Für Zeitzone, Sommerzeit und Wetter.", "Usata per fuso orario, ora legale e meteo."},
    {"Save and enable Wi-Fi", "Enregistrer et activer le Wi-Fi", "Guardar y activar Wi-Fi", "Speichern und Wi-Fi aktivieren", "Salva e attiva Wi-Fi"},
    {"Wi-Fi name and city are required.", "Le nom du Wi-Fi et la ville sont requis.", "El nombre del Wi-Fi y la ciudad son obligatorios.", "Wi-Fi-Name und Stadt sind erforderlich.", "Nome Wi-Fi e città sono obbligatori."},
    {"Saved", "Enregistré", "Guardado", "Gespeichert", "Salvato"},
    {"Return to Maclock and press Back.", "Revenez à Maclock et appuyez sur Retour.", "Vuelva a Maclock y pulse Atrás.", "Zu Maclock zurückkehren und Zurück drücken.", "Torna a Maclock e premi Indietro."},
    {"Touch the crosshair", "Touchez la croix", "Toque la cruz", "Kreuz berühren", "Tocca la croce"},
    {"Calibration failed - try again", "Échec du calibrage - réessayez", "Calibración fallida - reintente", "Kalibrierung fehlgeschlagen", "Calibrazione fallita - riprova"},
    {"In", "Int", "Int", "Innen", "Int"},
    {"Out", "Ext", "Ext", "Außen", "Est"},
    {"Today", "Auj.", "Hoy", "Heute", "Oggi"},
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
    {"Mon", "Lun", "Lun", "Mon", "Lun"},
    {"Tue", "Mar", "Mar", "Die", "Mar"},
    {"Wed", "Mer", "Mié", "Mit", "Mer"},
    {"Thu", "Jeu", "Jue", "Don", "Gio"},
    {"Fri", "Ven", "Vie", "Fre", "Ven"},
    {"Sat", "Sam", "Sáb", "Sam", "Sab"},
    {"Sun", "Dim", "Dom", "Son", "Dom"},
    {"Snooze 9 min", "Rappel 9 min", "Posponer 9 min", "9 Min schlummern", "Rinvia 9 min"},
    {"Dismiss", "Fermer", "Descartar", "Schließen", "Chiudi"},
    {"Adjust duration", "Régler la durée", "Ajustar duración", "Dauer einstellen", "Regola durata"},
    {"Running in background", "Actif en arrière-plan", "Activo en segundo plano", "Läuft im Hintergrund", "Attivo in background"},
    {"Timer Complete", "Minuteur terminé", "Temporizador terminado", "Timer beendet", "Timer completato"},
    {"Press Clock or Alarm to dismiss", "Appuyez sur Horloge ou Alarme", "Pulse Reloj o Alarma", "Uhr oder Alarm drücken", "Premi Orologio o Sveglia"},
    {"Set Date/Time", "Régler date/heure", "Ajustar fecha/hora", "Datum/Uhrzeit stellen", "Imposta data/ora"},
    {"Play", "Lire", "Reproducir", "Abspielen", "Riproduci"},
    {"No MP3 files in LittleFS", "Aucun MP3 dans LittleFS", "Sin MP3 en LittleFS", "Keine MP3 in LittleFS", "Nessun MP3 in LittleFS"},
    {"Clock: Enter   Alarm: Escape", "Horloge: Entree  Alarme: Echap", "Reloj: Intro   Alarma: Escape", "Uhr: Eingabe   Alarm: Escape", "Orologio: Invio  Sveglia: Esc"},
    {"Hold both 2s: Boot Options", "Maintenir les deux 2s: Options", "Mantenga ambos 2s: Opciones", "Beide 2s halten: Startoptionen", "Tieni entrambi 2s: Opzioni"},
    {"Rotary: Brightness", "Molette: Luminosite", "Rueda: Brillo", "Drehknopf: Helligkeit", "Manopola: Luminosita"},
    {"I2C", "I2C", "I2C", "I2C", "I2C"},
    {"Wi-Fi", "Wi-Fi", "Wi-Fi", "Wi-Fi", "Wi-Fi"},
    {"SSID", "SSID", "SSID", "SSID", "SSID"},
    {"IP/RSSI", "IP/RSSI", "IP/RSSI", "IP/RSSI", "IP/RSSI"},
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
        "English", "Français", "Español", "Deutsch", "Italiano"};
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
