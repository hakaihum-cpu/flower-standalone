//! CA-72 Minimoog Model D circuit-derived engine for the EFFECTS Pd external.
//! GPL-3.0-or-later; original circuit research and DSP: Idle Foundry Ltd.

use ca72::voice::{Panel, Priority, Quality, Voice, Waveform};
use std::panic::{catch_unwind, AssertUnwindSafe};

const MAX_POLY: usize = 8;
struct Slot {
    voice: Voice,
    pitch: Option<i32>,
    age: u64,
    release_samples: u64,
}

pub struct Engine {
    slots: Vec<Slot>,
    sample_rate: f64,
    master: f64,
    gain: f64,
    age: u64,
}

impl Engine {
    fn new(sample_rate: f64, polyphony: usize) -> Self {
        let count = polyphony.clamp(1, MAX_POLY);
        let mut panel = Panel::default();
        // One entire modelled instrument per note: no shared voice oscillators.
        panel.quality = Quality::Potato;
        panel.priority = Priority::Low;
        panel.retrigger = true;
        panel.osc[0].on = true;
        panel.osc[1].on = true;
        panel.osc[2].on = true;
        panel.osc[0].volume = 0.65;
        panel.osc[1].volume = 0.55;
        panel.osc[2].volume = 0.50;
        let mut slots = Vec::with_capacity(count);
        for i in 0..count {
            let mut voice = Voice::new(sample_rate, panel);
            voice.start_oscillators_at(i as f64 / count as f64);
            slots.push(Slot { voice, pitch: None, age: 0, release_samples: 0 });
        }
        Self { slots, sample_rate, master: 0.35, gain: 1.0, age: 0 }
    }

    fn note(&mut self, pitch: i32, velocity: i32) {
        if !(0..=127).contains(&pitch) { return; }
        if velocity <= 0 {
            for s in &mut self.slots {
                if s.pitch == Some(pitch) {
                    s.voice.note(pitch, false);
                    s.release_samples = 1;
                }
            }
            return;
        }

        self.age = self.age.wrapping_add(1);
        let index = self.slots.iter().position(|s| s.pitch.is_none())
            .or_else(|| self.slots.iter().position(|s| s.release_samples > 0))
            .unwrap_or_else(|| self.slots.iter().enumerate()
                .min_by_key(|(_,s)| s.age).map_or(0, |(i,_)| i));
        let s = &mut self.slots[index];
        if let Some(old) = s.pitch { s.voice.note(old, false); }
        s.voice.note(pitch, true);
        s.pitch = Some(pitch);
        s.release_samples = 0;
        s.age = self.age;
    }

    fn parameter(&mut self, id: u32, value: f64) {
        if !value.is_finite() { return; }
        let v = value.clamp(0.0, 1.0);
        if id == 21 { self.master = 0.7*v; return; }
        if id == 22 { self.gain = 0.5+1.5*v; return; }
        for s in &mut self.slots {
            let p = &mut s.voice.panel;
            match id {
                0 => p.cutoff = v,
                1 => p.emphasis = v,
                2 => p.contour_amount = v,
                3 => p.filter_contour.attack = v,
                4 => p.filter_contour.decay = v,
                5 => p.filter_contour.sustain = v,
                6 => p.loudness_contour.attack = v,
                7 => p.loudness_contour.decay = v,
                8 => p.loudness_contour.sustain = v,
                9..=11 => {
                    p.osc[(id-9) as usize].waveform = match (v*5.9999) as u32 {
                        0 => Waveform::Triangle,
                        1 => Waveform::SharkTooth,
                        2 => Waveform::Sawtooth,
                        3 => Waveform::Square,
                        4 => Waveform::WideRectangle,
                        _ => Waveform::NarrowRectangle,
                    }
                },
                12..=14 => p.osc[(id-12) as usize].volume = v,
                15 => p.noise_volume = v,
                16 => p.noise_on = v > 0.5,
                17 => p.mod_mix = v,
                18 => p.filter_mod = v > 0.5,
                19 => p.osc_mod = v > 0.5,
                20 => p.glide = v,
                _ => {}
            }
        }
    }

    fn process(&mut self, l: &mut [f32], r: &mut [f32]) {
        let scaling = self.master / (self.slots.len() as f64).sqrt();
        for (left, right) in l.iter_mut().zip(r.iter_mut()) {
            let mut mix = 0.0;
            for s in &mut self.slots {
                if s.pitch.is_none() { continue; }
                mix += s.voice.tick();
                if s.release_samples > 0 {
                    s.release_samples += 1;
                    let (_, env, held) = s.voice.envelopes();
                    if !held && env.abs() < 1e-4
                        && s.release_samples > (self.sample_rate*0.02) as u64 {
                        s.pitch = None;
                        s.release_samples = 0;
                    }
                }
            }
            let value = (mix * scaling * self.gain).clamp(-1.0, 1.0) as f32;
            *left = value;
            *right = value;
        }
    }
}

// Opaque pointers are exclusively borrowed by libpd. New/free are not invoked
// by its realtime audio callback.
#[unsafe(no_mangle)]
pub extern "C" fn ca72_pd_new(rate: f64, poly: u32) -> *mut Engine {
    if !rate.is_finite() || !(8000.0..=192000.0).contains(&rate) {
        return std::ptr::null_mut();
    }
    match catch_unwind(AssertUnwindSafe(|| Box::new(Engine::new(rate, poly as usize)))) {
        Ok(engine) => Box::into_raw(engine),
        Err(_) => std::ptr::null_mut(),
    }
}

/// # Safety
/// p must have been returned by ca72_pd_new, not yet freed.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ca72_pd_free(p: *mut Engine) {
    if !p.is_null() { unsafe { drop(Box::from_raw(p)); } }
}

/// # Safety
/// p must be a live exclusively borrowed engine pointer.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ca72_pd_note(p: *mut Engine, pitch: i32, velocity: i32) {
    if let Some(p) = unsafe { p.as_mut() } { p.note(pitch,velocity); }
}

/// # Safety
/// p must be a live exclusively borrowed engine pointer.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ca72_pd_parameter(p: *mut Engine, id: u32, value: f64) {
    if let Some(p) = unsafe { p.as_mut() } { p.parameter(id,value); }
}

/// # Safety
/// p must be exclusively borrowed; l and r are distinct arrays of at least
/// n valid f32 samples. No graph mutation or disk I/O occurs here.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ca72_pd_process(p: *mut Engine, l: *mut f32, r: *mut f32, n: i32) {
    if n <= 0 || p.is_null() || l.is_null() || r.is_null() { return; }
    let Some(p) = (unsafe { p.as_mut() }) else { return; };
    let l = unsafe { std::slice::from_raw_parts_mut(l, n as usize) };
    let r = unsafe { std::slice::from_raw_parts_mut(r, n as usize) };
    p.process(l,r);
}
