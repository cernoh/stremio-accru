use serde_json::Value;

/// Smart Track Selector — language priority, forced-override, rejection lists.
/// Mock: selects best audio/subs based on prefs.
#[derive(Debug, Clone)]
pub struct TrackPrefs {
    pub audio_lang: String,
    pub subs_lang: String,
    pub forced_override: bool,
    pub reject_langs: Vec<String>,
    pub reject_keywords: Vec<String>,
}

impl Default for TrackPrefs {
    fn default() -> Self {
        Self {
            audio_lang: "en".into(),
            subs_lang: "en".into(),
            forced_override: true,
            reject_langs: vec![],
            reject_keywords: vec!["SDH".into()],
        }
    }
}

pub fn select_best(tracks: &[Value], prefs: &TrackPrefs, kind: &str) -> Option<Value> {
    let mut candidates: Vec<&Value> = tracks
        .iter()
        .filter(|t| t.get("type").and_then(Value::as_str) == Some(kind))
        .filter(|t| {
            let lang = t.get("lang").and_then(Value::as_str).unwrap_or("");
            !prefs.reject_langs.iter().any(|l| l == lang)
        })
        .filter(|t| {
            let title = t.get("title").and_then(Value::as_str).unwrap_or("");
            !prefs.reject_keywords.iter().any(|k| title.contains(k))
        })
        .collect();

    // Prefer exact lang match, then forced, then embedded > external
    candidates.sort_by_key(|t| {
        let lang = t.get("lang").and_then(Value::as_str).unwrap_or("");
        let is_forced = t.get("forced").and_then(Value::as_bool).unwrap_or(false);
        let is_embedded = t
            .get("external")
            .and_then(Value::as_bool)
            .map(|e| !e)
            .unwrap_or(true);
        (
            if lang == prefs.audio_lang { 0 } else { 1 },
            if is_forced && prefs.forced_override {
                0
            } else {
                1
            },
            if is_embedded { 0 } else { 1 },
        )
    });
    candidates.first().cloned().cloned()
}
