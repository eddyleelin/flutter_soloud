// ignore_for_file: public_member_api_docs

import 'package:flutter_soloud/src/filters/filters.dart';
import 'package:flutter_soloud/src/soloud.dart';
import 'package:flutter_soloud/src/sound_handle.dart';
import 'package:flutter_soloud/src/sound_hash.dart';

enum PitchShiftEnum {
  wet,
  shift,
  semitones,
  algorithm;

  final List<double> _mins = const [0, 0, -36, 0];
  final List<double> _maxs = const [1, 3, 36, 1];
  final List<double> _defs = const [1, 1, 0, 0];

  double get min => _mins[index];
  double get max => _maxs[index];
  double get def => _defs[index];

  @override
  String toString() => switch (this) {
        PitchShiftEnum.wet => 'Wet',
        PitchShiftEnum.shift => 'Shift',
        PitchShiftEnum.semitones => 'Semitones',
        PitchShiftEnum.algorithm => 'Algorithm',
      };
}

enum PitchShiftAlgorithm {
  signalsmith,
  soundTouch;

  double get value => index.toDouble();
}

abstract class _PitchShiftInternal extends FilterBase {
  const _PitchShiftInternal(SoundHash? soundHash)
      : super(FilterType.pitchShiftFilter, soundHash);

  PitchShiftEnum get queryWet => PitchShiftEnum.wet;
  PitchShiftEnum get queryShift => PitchShiftEnum.shift;
  PitchShiftEnum get querySemitones => PitchShiftEnum.semitones;
  PitchShiftEnum get queryAlgorithm => PitchShiftEnum.algorithm;
}

class PitchShiftSingle extends _PitchShiftInternal {
  PitchShiftSingle(super.soundHash);

  FilterParam wet({SoundHandle? soundHandle}) => FilterParam(
        soundHandle,
        filterType,
        PitchShiftEnum.wet.index,
        PitchShiftEnum.wet.min,
        PitchShiftEnum.wet.max,
      );

  /// The shift value of the pitch, where 1.0 means the pitch is not shifted.
  ///
  /// Note that both the [shift] and [semitones] parameters are acting to modify
  /// the pitch value, but using different scales. Changing this value will
  /// therefore adjust the [shift] parameter with:
  /// ```dart
  /// shift = pow(2., value / 12);
  /// ```
  FilterParam shift({SoundHandle? soundHandle}) => FilterParam(
        soundHandle,
        filterType,
        PitchShiftEnum.shift.index,
        PitchShiftEnum.shift.min,
        PitchShiftEnum.shift.max,
      );

  /// The number of semitones that the pitch is shifted.
  ///
  /// Note that both the [shift] and [semitones] parameters are acting to modify
  /// the pitch value, but using different scales. Changing this value will
  /// therefore adjust the [semitones] parameter with:
  /// ```dart
  /// semitones = 12 * log2f(value);
  /// ```
  FilterParam semitones({SoundHandle? soundHandle}) => FilterParam(
        soundHandle,
        filterType,
        PitchShiftEnum.semitones.index,
        PitchShiftEnum.semitones.min,
        PitchShiftEnum.semitones.max,
      );

  /// The pitch shifting algorithm to use.
  ///
  /// - 0: Signalsmith (default, MIT)
  /// - 1: SoundTouch (LGPL)
  FilterParam algorithm({SoundHandle? soundHandle}) => FilterParam(
        soundHandle,
        filterType,
        PitchShiftEnum.algorithm.index,
        PitchShiftEnum.algorithm.min,
        PitchShiftEnum.algorithm.max,
      );

  /// Adjust the play speed of a sound without changing the pitch of the audio.
  ///
  /// This is done by counteracting the change in pitch caused by changing the
  /// speed using the [shift] parameter.
  void timeStretch(SoundHandle soundHandle, double value) {
    // Adjust the play speed
    SoLoud.instance.setRelativePlaySpeed(soundHandle!, value);
    shift(soundHandle: soundHandle).value = 1.0 / value;
  }
}

class PitchShiftGlobal extends _PitchShiftInternal {
  const PitchShiftGlobal() : super(null);

  FilterParam get wet => FilterParam(
        null,
        filterType,
        PitchShiftEnum.wet.index,
        PitchShiftEnum.wet.min,
        PitchShiftEnum.wet.max,
      );

  /// The shift value of the pitch, where 1.0 means the pitch is not shifted.
  ///
  /// Note that both the [shift] and [semitones] parameters are acting to modify
  /// the pitch value, but using different scales. Changing this value will
  /// therefore adjust the [shift] parameter with:
  /// ```dart
  /// shift = pow(2., value / 12);
  /// ```
  FilterParam get shift => FilterParam(
        null,
        filterType,
        PitchShiftEnum.shift.index,
        PitchShiftEnum.shift.min,
        PitchShiftEnum.shift.max,
      );

  /// The number of semitones that the pitch is shifted.
  ///
  /// Note that both the [shift] and [semitones] parameters are acting to modify
  /// the pitch value, but using different scales. Changing this value will
  /// therefore adjust the [semitones] parameter with:
  /// ```dart
  /// semitones = 12 * log2f(value);
  /// ```
  FilterParam get semitones => FilterParam(
        null,
        filterType,
        PitchShiftEnum.semitones.index,
        PitchShiftEnum.semitones.min,
        PitchShiftEnum.semitones.max,
      );

  /// The pitch shifting algorithm to use.
  ///
  /// - 0: Signalsmith (default, MIT)
  /// - 1: SoundTouch (LGPL)
  FilterParam get algorithm => FilterParam(
        null,
        filterType,
        PitchShiftEnum.algorithm.index,
        PitchShiftEnum.algorithm.min,
        PitchShiftEnum.algorithm.max,
      );
}
