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
    // Use the correct language pref per kind: audio_lang for audio, subs_lang for subs.
    let wanted_lang = match kind {
        "audio" => prefs.audio_lang.as_str(),
        "subs" | "subtitles" | "captions" => prefs.subs_lang.as_str(),
        _ => prefs.audio_lang.as_str(),
    };
    candidates.sort_by_key(|t| {
        let lang = t.get("lang").and_then(Value::as_str).unwrap_or("");
        let is_forced = t.get("forced").and_then(Value::as_bool).unwrap_or(false);
        let is_embedded = t
            .get("external")
            .and_then(Value::as_bool)
            .map(|e| !e)
            .unwrap_or(true);
        (
            if lang == wanted_lang { 0 } else { 1 },
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

#[cfg(test)]
mod tests {
    use super::*;
    use serde_json::json;

    fn mk_track(lang: &str, kind: &str, forced: bool, external: bool, title: &str) -> serde_json::Value {
        json!({ "lang": lang, "type": kind, "forced": forced, "external": external, "title": title })
    }

    #[test]
    fn select_best_exact_lang_match() {
        let tracks = vec![
            mk_track("en", "audio", false, false, "English"),
            mk_track("ja", "audio", false, false, "Japanese"),
        ];
        let prefs = TrackPrefs { audio_lang: "ja".into(), ..Default::default() };
        let best = select_best(&tracks, &prefs, "audio").unwrap();
        assert_eq!(best["lang"], "ja");
    }

    #[test]
    fn forced_override_priority() {
        let tracks = vec![
            mk_track("en", "audio", false, false, "English"),
            mk_track("en", "audio", true, false, "English Forced"),
        ];
        let prefs = TrackPrefs { audio_lang: "en".into(), forced_override: true, ..Default::default() };
        let best = select_best(&tracks, &prefs, "audio").unwrap();
        assert_eq!(best["forced"], true);
    }

    #[test]
    fn embedded_preferred_over_external() {
        let tracks = vec![
            mk_track("en", "audio", false, true, "External"),
            mk_track("en", "audio", false, false, "Embedded"),
        ];
        let prefs = TrackPrefs::default();
        let best = select_best(&tracks, &prefs, "audio").unwrap();
        assert_eq!(best["external"], false);
    }

    #[test]
    fn reject_langs_filtered() {
        let tracks = vec![
            mk_track("en", "audio", false, false, "English"),
            mk_track("fr", "audio", false, false, "French"),
        ];
        let prefs = TrackPrefs { reject_langs: vec!["en".into()], ..Default::default() };
        let best = select_best(&tracks, &prefs, "audio").unwrap();
        assert_eq!(best["lang"], "fr");
    }

    #[test]
    fn reject_keywords_filtered() {
        let tracks = vec![
            mk_track("en", "subs", false, false, "English SDH"),
            mk_track("en", "subs", false, false, "English"),
        ];
        let prefs = TrackPrefs::default(); // rejects SDH by default
        let best = select_best(&tracks, &prefs, "subs").unwrap();
        assert_eq!(best["title"], "English");
    }

    #[test]
    fn filter_by_kind() {
        let tracks = vec![
            mk_track("en", "audio", false, false, "Audio"),
            mk_track("en", "subs", false, false, "Subs"),
        ];
        let prefs = TrackPrefs::default();
        assert!(select_best(&tracks, &prefs, "video").is_none());
        assert_eq!(select_best(&tracks, &prefs, "audio").unwrap()["type"], "audio");
    }

    #[test]
    fn none_when_all_rejected() {
        let tracks = vec![mk_track("en", "audio", false, false, "English SDH")];
        let prefs = TrackPrefs::default();
        // SDH title triggers reject_keywords -> none
        assert!(select_best(&tracks, &prefs, "audio").is_none());
    }

    #[test]
    fn subs_uses_subs_lang_not_audio_lang() {
        // Bug exposed: old code sorted subs by audio_lang unconditionally.
        // With audio_lang=en, subs_lang=ja, subs tracks ja/en, should pick ja.
        let tracks = vec![
            mk_track("en", "subs", false, false, "English Subs"),
            mk_track("ja", "subs", false, false, "Japanese Subs"),
        ];
        let prefs = TrackPrefs {
            audio_lang: "en".into(),
            subs_lang: "ja".into(),
            ..Default::default()
        };
        let best = select_best(&tracks, &prefs, "subs").unwrap();
        assert_eq!(best["lang"], "ja", "subs selection must use subs_lang, not audio_lang");
    }

    #[test]
    fn audio_still_uses_audio_lang_when_subs_differ() {
        let tracks = vec![
            mk_track("en", "audio", false, false, "English Audio"),
            mk_track("ja", "audio", false, false, "Japanese Audio"),
        ];
        let prefs = TrackPrefs {
            audio_lang: "en".into(),
            subs_lang: "ja".into(),
            ..Default::default()
        };
        let best = select_best(&tracks, &prefs, "audio").unwrap();
        assert_eq!(best["lang"], "en", "audio selection must use audio_lang");
    }
}
