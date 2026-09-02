use std::collections::HashMap;

use parking_lot::RwLock;
use serde_json::{json, Value};

/// Mock of stremio-core Runtime (Elm arch) for M2.
/// Real impl would be `stremio_core::runtime::Runtime` with `Env` trait.
/// This mock stores catalogs, meta, addons in-memory and supports
/// `dispatch_action` → `NewState` flow via Tauri events.

#[derive(Default)]
pub struct CoreRuntime {
    // In real core: `Ctx` + models, driven by `Action` → `Msg` → `Effects` → `Env`
    state: RwLock<Value>,
    addons: RwLock<Vec<Value>>,
    catalogs: RwLock<HashMap<String, Value>>,
}

impl CoreRuntime {
    pub fn new() -> Self {
        let mut s = Self::default();
        s.init_sample();
        s
    }

    fn init_sample(&mut self) {
        let mut catalogs = HashMap::new();
        catalogs.insert(
            "movie:popular".to_string(),
            json!({
                "id": "movie:popular",
                "name": "Popular Movies",
                "type": "movie",
                "items": [
                    {"id":"tt0133093","name":"The Matrix","type":"movie","poster":"https://via-imdb.com/matrix.jpg","year":"1999"},
                    {"id":"tt1375666","name":"Inception","type":"movie","poster":"https://via-imdb.com/inception.jpg","year":"2010"}
                ]
            }),
        );
        catalogs.insert(
            "series:popular".to_string(),
            json!({
                "id": "series:popular",
                "name": "Popular Series",
                "type": "series",
                "items": [
                    {"id":"tt0903747","name":"Breaking Bad","type":"series","poster":"https://via-imdb.com/bb.jpg","year":"2008"},
                    {"id":"tt2861424","name":"Rick and Morty","type":"series","poster":"https://via-imdb.com/rm.jpg","year":"2013"}
                ]
            }),
        );
        *self.catalogs.write() = catalogs;
        *self.state.write() = json!({
            "ctx": {"profile": {"auth": null}, "addons": [], "library": []},
            "catalogs": [],
            "metaDetails": null,
            "player": null,
        });
        *self.addons.write() = vec![
            json!({"id":"com.linvo.cinemeta","name":"Cinemeta","url":"https://v3-cinemeta.strem.io/manifest.json","installed":true}),
            json!({"id":"com.snoak","name":"Snoak","url":"https://snoak.example/manifest.json","installed":false}),
        ];
    }

    pub fn get_state(&self) -> Value {
        let catalogs = self.catalogs.read();
        let addons = self.addons.read();
        let base = self.state.read();
        json!({
            "ctx": base["ctx"],
            "addons": *addons,
            "catalogs": catalogs.values().cloned().collect::<Vec<_>>(),
            "metaDetails": base["metaDetails"],
            "player": base["player"],
        })
    }

    pub fn dispatch(&self, action: Value) -> Value {
        let typ = action.get("type").and_then(Value::as_str).unwrap_or("");
        tracing::info!(target: "core", "dispatch {typ} {action}");
        match typ {
            "LoadCatalog" => {
                let id = action
                    .get("id")
                    .and_then(Value::as_str)
                    .unwrap_or("movie:popular");
                let catalog = self
                    .catalogs
                    .read()
                    .get(id)
                    .cloned()
                    .unwrap_or(json!({"id": id, "items": []}));
                json!({"type": "NewState", "state": self.get_state(), "catalog": catalog})
            }
            "GetMeta" => {
                let id = action
                    .get("id")
                    .and_then(Value::as_str)
                    .unwrap_or("tt0133093");
                let meta = json!({
                    "id": id,
                    "name": "Sample Title",
                    "type": "movie",
                    "year": "1999",
                    "poster": "https://via-imdb.com/poster.jpg",
                    "background": "https://via-imdb.com/bg.jpg",
                    "description": "Mock meta from stremio-core Env (fetch/reqwest would hit Cinemeta).",
                    "cast": [{"name":"Keanu Reeves","character":"Neo","photo":"https://via-imdb.com/keanu.jpg"}],
                    "videos": [{"id": format!("{id}:1:1"), "title": "Episode 1", "released": "2024-01-01"}],
                    "ratings": {"imdb": 8.7, "tmdb": 8.5}
                });
                json!({"type": "NewState", "meta": meta, "state": self.get_state()})
            }
            "ResolveStream" => {
                // In real core: addon_transport → stream URL, then StreamingServer
                let id = action
                    .get("id")
                    .and_then(Value::as_str)
                    .unwrap_or("tt0133093");
                let stream = json!({
                    "url": format!("https://example.com/stream/{id}.mp4"),
                    "behaviorHints": {"bingeGroup": format!("{id}")},
                    "title": "1080p • Mock"
                });
                json!({"type": "CoreEvent", "event": "StreamResolved", "stream": stream})
            }
            "InstallAddon" => {
                let url = action.get("url").and_then(Value::as_str).unwrap_or("");
                self.addons
                    .write()
                    .push(json!({"id": url, "url": url, "installed": true}));
                json!({"type": "NewState", "state": self.get_state()})
            }
            _ => json!({"type": "NewState", "state": self.get_state()}),
        }
    }
}
