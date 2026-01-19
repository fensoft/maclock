#pragma once
#include <Arduino.h>

class TouchSensor {
public:
  explicit TouchSensor(int gpio) : _gpio(gpio) {}

  void begin() {
    _state = false;
    _dir = 0;
    _onCnt = _offCnt = 0;
    _dirCntPos = _dirCntNeg = 0;

    // Build an initial baseline from valid samples only
    uint32_t b = readAvgValid(60);
    _baseline = (b == 0) ? 100000.0f : (float)b;
    _raw = b;
  }

  // Call every ~5–20ms
  bool update() {
    uint32_t raw = readAvgValid(_avgSamples);
    if (raw == 0) {
      // No valid measurement: don't change state, but also don't force "touched"
      return _state;
    }
    _raw = raw;

    // If dir unknown: keep tracking baseline and learn direction with confidence
    if (_dir == 0) {
      // Track baseline freely while we don't know direction
      _baseline = (1.0f - _alphaLearn) * _baseline + _alphaLearn * raw;

      int32_t d = (int32_t)raw - (int32_t)_baseline;
      int32_t learnThr = (int32_t)(_baseline * _learnFrac);

      if (d > learnThr) {
        _dirCntPos++;
        _dirCntNeg = 0;
      } else if (d < -learnThr) {
        _dirCntNeg++;
        _dirCntPos = 0;
      } else {
        // close to baseline: decay counters
        if (_dirCntPos > 0) _dirCntPos--;
        if (_dirCntNeg > 0) _dirCntNeg--;
      }

      if (_dirCntPos >= _learnCount) _dir = +1;
      if (_dirCntNeg >= _learnCount) _dir = -1;

      // While direction is unknown, never assert touch
      _state = false;
      _onCnt = _offCnt = 0;
      return _state;
    }

    // Normal touch evaluation with hysteresis
    bool nowTouched = evaluate(raw);

    // Debounce
    if (nowTouched) {
      _onCnt++;
      _offCnt = 0;
      if (!_state && _onCnt >= _debounceOn) {
        _state = true;
        _onCnt = 0;
      }
    } else {
      _offCnt++;
      _onCnt = 0;
      if (_state && _offCnt >= _debounceOff) {
        _state = false;
        _offCnt = 0;
      }
    }

    // Auto-calibrate baseline only when not touched
    if (!_state) {
      _baseline = (1.0f - _alpha) * _baseline + _alpha * raw;
    }

    // Safety: if signal looks "stuck" on wrong side for long, re-learn direction
    if (_state) {
      int32_t d = (int32_t)raw - (int32_t)_baseline;
      int32_t bigOpp = (int32_t)(_baseline * 0.20f);
      if ((_dir > 0 && d < -bigOpp) || (_dir < 0 && d > bigOpp)) {
        _dir = 0;
        _dirCntPos = _dirCntNeg = 0;
        _state = false;
      }
    }

    return _state;
  }

  bool touched() const { return _state; }
  uint32_t raw() const { return _raw; }
  uint32_t baseline() const { return (uint32_t)_baseline; }
  int direction() const { return _dir; } // +1 up, -1 down, 0 unknown

private:
  const int _gpio;

  // --- tuning ---
  const int   _avgSamples  = 4;

  const float _onFrac      = 0.08f;  // touch threshold (8%)
  const float _offFrac     = 0.04f;  // release hysteresis (4%)

  const float _alpha       = 0.02f;  // baseline tracking when not touched
  const float _alphaLearn  = 0.05f;  // baseline tracking while learning direction

  const float _learnFrac   = 0.06f;  // direction-learning threshold (6%)
  const int   _learnCount  = 6;      // consecutive samples to lock direction

  const int   _debounceOn  = 4;
  const int   _debounceOff = 6;

  // --- state ---
  float _baseline = 100000.0f;
  uint32_t _raw = 0;
  bool _state = false;

  int _dir = 0;
  int _dirCntPos = 0, _dirCntNeg = 0;
  int _onCnt = 0, _offCnt = 0;

  // Read average, ignoring invalid readings like 0x3FFFFF
  uint32_t readAvgValid(int n) {
    uint64_t s = 0;
    int good = 0;
    for (int i = 0; i < n; i++) {
      uint32_t v = touchRead(_gpio);
      // ESP32-S3 "invalid/max" seen as 0x3FFFFF (4194303)
      if (v != 0x3FFFFF && v != 0) {
        s += v;
        good++;
      }
      delayMicroseconds(200);
    }
    if (good == 0) return 0;
    return (uint32_t)(s / (uint64_t)good);
  }

  bool evaluate(uint32_t raw) {
    int32_t d = (int32_t)raw - (int32_t)_baseline;
    int32_t onThr  = (int32_t)(_baseline * _onFrac);
    int32_t offThr = (int32_t)(_baseline * _offFrac);

    if (!_state) {
      return (_dir > 0) ? (d >  onThr) : (d < -onThr);
    } else {
      return (_dir > 0) ? (d >  offThr) : (d < -offThr);
    }
  }
};
