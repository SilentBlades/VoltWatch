#ifndef VOLTWATCH_SECRETS_EXAMPLE_H
#define VOLTWATCH_SECRETS_EXAMPLE_H

// =============================================================================
// VoltWatch - Secrets Configuration Template
// =============================================================================
// Copy this file to "secrets.h" in the same directory:
//   cp firmware/include/secrets.example.h firmware/include/secrets.h
//
// Then update the values below with your credentials.
// Note: "secrets.h" is ignored by git and will never be committed.
// =============================================================================

// --- Wi-Fi Credentials ---
#define WIFI_SSID         "Your_Home_WiFi_SSID"
#define WIFI_PASSWORD     "Your_WiFi_Password"

// --- Google SMTP Credentials ---
// Enter your 16-character Google App Password (without spaces)
#define SMTP_SENDER_NAME  "VoltWatch Sentinel"
#define SMTP_SENDER_EMAIL "your.email@gmail.com"
#define SMTP_APP_PASSWORD "xxxx xxxx xxxx xxxx"

// --- Alert Recipients ---
// Configure 1 to 3 recipient email addresses. Leave blank ("") if not needed.
#define SMTP_RECIPIENT_1  "primary.phone@gmail.com"
#define SMTP_RECIPIENT_2  "secondary.spouse@gmail.com"
#define SMTP_RECIPIENT_3  ""

#endif // VOLTWATCH_SECRETS_EXAMPLE_H
