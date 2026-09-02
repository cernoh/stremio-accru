use anyhow::Result;
use serde_json::Value;

use super::LoadOpts;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Anime4KPreset {
    Optimized,
    Fast,
    HQ,
    Off,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum VisualProfile {
    Kai,
    Vivid,
    Original,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum AudioPreset {
    Off,
    Night,
    Voice,
}

#[async_trait::async_trait]
pub trait PlayerBackend: Send + Sync {
    fn load(&self, url: &str, opts: LoadOpts) -> Result<()>;
    fn set_property(&self, key: &str, val: Value) -> Result<()>;
    fn observe(&self, key: &str) -> Result<()>;
    fn command(&self, cmd: &str, args: &[&str]) -> Result<()>;
    fn set_shader_preset(&self, preset: Anime4KPreset) -> Result<()> {
        let _ = preset;
        Ok(())
    }
    fn set_visual_profile(&self, profile: VisualProfile) -> Result<()> {
        let _ = profile;
        Ok(())
    }
    fn set_audio_preset(&self, preset: AudioPreset) -> Result<()> {
        let _ = preset;
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn anime4k_preset_eq() {
        assert_eq!(Anime4KPreset::Optimized, Anime4KPreset::Optimized);
        assert_ne!(Anime4KPreset::Off, Anime4KPreset::HQ);
    }

    #[test]
    fn visual_profile_eq() {
        assert_eq!(VisualProfile::Kai, VisualProfile::Kai);
        assert_ne!(VisualProfile::Vivid, VisualProfile::Original);
    }

    #[test]
    fn audio_preset_eq() {
        assert_eq!(AudioPreset::Night, AudioPreset::Night);
        assert_ne!(AudioPreset::Off, AudioPreset::Voice);
    }
}
