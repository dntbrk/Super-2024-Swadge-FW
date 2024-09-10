#pragma once

/*! \file midiPlayer.h
 *
 * \section midiPlayer_design Design Philosophy
 *
 * This code is provided to play MIDI songs via the DAC speaker. MIDI songs are loaded
 * with midiFileParser.h.
 *
 * Two system-wide MIDI players are provided by default and can be used to playback
 * BGM and SFX independently. They will automatically be initialized if ::swadgeMode_t.fnDacCb
 * is \c NULL. Each MIDI player supports 16 channels and can simultaneously play up to
 * 24 melodic notes and 8 percussion notes, shared across all channels. All 128 General MIDI
 * instruments are supported, as well as the full General MIDI percussion range.
 *
 * \code{.c}
 * // Load a MIDI file
 * midiFile_t ode_to_joy;
 * loadMidiFile("ode.mid", &ode_to_joy, true);
 *
 * // Play the song on the BGM channel
 * globalMidiPlayerPlaySong(&ode_to_joy, MIDI_BGM);
 *
 * // Pause both BGM and SFX
 * globalMidiPauseAll();
 *
 * // Unpause
 * globalMidiUnpauseAll();
 *
 * // Stop and save the player state
 * void* data = globalMidiSave();
 *
 * // Restore the player state
 * globalMidiRestore(data);
 *
 * // Get a pointer to a particular global MIDI player for advanced usage
 * midiPlayer_t* player = globalMidiPlayerGet(MIDI_SFX)
 *
 * // Turn on the sustain pedal for channel 1
 * midiSustain(player, 0, 0x7F);
 *
 * // Pitch-bend channel 3 down by one half-step
 * midiPitchWheel(player, 2, 0x1000);
 *
 * // Play an A3 note at half velocity on channel 3
 * midiNoteOn(player, 2, 69, 0x40);
 *
 * // Turn the note off
 * midiNoteOff(player, 2, 69, 0x7F);
 *
 * // Play a drum kit sound at full velocity on channel 10, which is reserved for percussion
 * midiNoteOn(player, 9, ACOUSTIC_SNARE, 0x7F);
 * \endcode
 */

//==============================================================================
// Includes
//==============================================================================

#include "swSynth.h"
#include "midiFileParser.h"

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

//==============================================================================
// Defines
//==============================================================================

#define MIDI_CHANNEL_COUNT 16
// The total number of pooled voices
#define POOL_VOICE_COUNT 24
// The number of voices reserved for percussion
#define PERCUSSION_VOICES 8
// The number of oscillators each voice gets. Maybe we'll need more than one for like, chorus?
#define OSC_PER_VOICE 1
// The number of global MIDI players
#define NUM_GLOBAL_PLAYERS 2
// The index of the system-wide MIDI player for sound effects
#define MIDI_SFX 0
// The index of the system-wide MIDI player for background music
#define MIDI_BGM 1
// The maximum volume setting for globalMidiPlayerSetVolume()
#define MAX_VOLUME 13

#define MIDI_TRUE         0x7F
#define MIDI_FALSE        0x00
#define MIDI_TO_BOOL(val) (val > 63)
#define BOOL_TO_MIDI(val) (val ? MIDI_TRUE : MIDI_FALSE)
#define MIDI_DEF_HEADROOM 0x2666
#define PITCH_BEND_CENTER 0x2000

/// @brief Convert the sample count to MIDI ticks
#define SAMPLES_TO_MIDI_TICKS(n, tempo, div) ((n) * 1000000 * (div) / DAC_SAMPLE_RATE_HZ / (tempo))

/// @brief Calculate the number of DAC samples in the given number of milliseconds
#define MS_TO_SAMPLES(ms) ((ms) * DAC_SAMPLE_RATE_HZ / 1000)

/// @brief Callback function used to provide feedback when a song finishes playing
typedef void (*songFinishedCbFn)(void);

//==============================================================================
// Enums
//==============================================================================

/**
 * @brief Represents the source of MIDI data
 *
 */
typedef enum
{
    /// @brief Streaming over USB
    MIDI_STREAMING,

    /// @brief Reading from a \c midiFileReader_t
    MIDI_FILE,
} midiPlayerMode_t;

/**
 * @brief The sample source for an instrument
 */
typedef enum
{
    /// @brief Samples are generated by sampling a particular wave shape
    WAVETABLE,
    SAMPLE,
    NOISE,
    WAVE_SHAPE,
} timbreType_t;

/**
 * @brief A bitfield which may contain various flags for a timbre
 */
typedef enum
{
    /// @brief No flags
    TF_NONE = 0,
    /// @brief This timbre plays percussion sounds (percussionNote_t) rather than melodic notes
    TF_PERCUSSION = 1,
    /// @brief This timbre represents a monophonic instrument
    TF_MONO = 2,
} timbreFlags_t;

/**
 * @brief Defines the MIDI note numbers mapped to by the General MIDI percussion note names
 */
typedef enum
{
    // Roland GS Extensions
    /*HIGH_Q_OR_FILTER_SNAP = 27,
    SLAP_NOISE = 28,
    SCRATCH_PUSH = 29,
    SCRATCH_PULL = 30,
    DRUM_STICKS = 31,
    SQUARE_CLICK = 32,
    METRONOME_CLICK = 33,
    METRONOME_BELL = 34,*/
    // End Roland GS Extensions
    ACOUSTIC_BASS_DRUM_OR_LOW_BASS_DRUM  = 35,
    ELECTRIC_BASS_DRUM_OR_HIGH_BASS_DRUM = 36,
    SIDE_STICK                           = 37,
    ACOUSTIC_SNARE                       = 38,
    HAND_CLAP                            = 39,
    ELECTRIC_SNARE_OR_RIMSHOT            = 40,
    LOW_FLOOR_TOM                        = 41,
    /// @brief This note supersedes any ::CLOSED_HI_HAT, ::PEDAL_HI_HAT, or ::OPEN_HI_HAT notes playing
    CLOSED_HI_HAT  = 42,
    HIGH_FLOOR_TOM = 43,
    /// @brief This note supersedes any ::CLOSED_HI_HAT, ::PEDAL_HI_HAT, or ::OPEN_HI_HAT notes playing
    PEDAL_HI_HAT = 44,
    LOW_TOM      = 45,
    /// @brief This note supersedes any ::CLOSED_HI_HAT, ::PEDAL_HI_HAT, or ::OPEN_HI_HAT notes playing
    OPEN_HI_HAT     = 46,
    LOW_MID_TOM     = 47,
    HIGH_MID_TOM    = 48,
    CRASH_CYMBAL_1  = 49,
    HIGH_TOM        = 50,
    RIDE_CYMBAL_1   = 51,
    CHINESE_CYMBAL  = 52,
    RIDE_BELL       = 53,
    TAMBOURINE      = 54,
    SPLASH_CYMBAL   = 55,
    COWBELL         = 56,
    CRASH_CYMBAL_2  = 57,
    VIBRASLAP       = 58,
    RIDE_CYMBAL_2   = 59,
    HIGH_BONGO      = 60,
    LOW_BONGO       = 61,
    MUTE_HIGH_CONGA = 62,
    OPEN_HIGH_CONGA = 63,
    LOW_CONGA       = 64,
    HIGH_TIMBALE    = 65,
    LOW_TIMBALE     = 66,
    HIGH_AGOGO      = 67,
    LOW_AGOGO       = 68,
    CABASA          = 69,
    MARACAS         = 70,
    /// @brief This note supersdes any ::SHORT_WHISTLE or ::LONG_WHISTLE notes playing
    SHORT_WHISTLE = 71,
    /// @brief This note supersdes any ::SHORT_WHISTLE or ::LONG_WHISTLE notes playing
    LONG_WHISTLE = 72,
    /// @brief This note supersdes any ::SHORT_GUIRO or ::LONG_GUIRO notes playing
    SHORT_GUIRO = 73,
    /// @brief This note supersdes any ::SHORT_GUIRO or ::LONG_GUIRO notes playing
    LONG_GUIRO     = 74,
    CLAVES         = 75,
    HIGH_WOODBLOCK = 76,
    LOW_WOODBLOCK  = 77,
    /// @brief This note supersdes any ::SHORT_GUIRO or ::LONG_GUIRO notes playing
    MUTE_CUICA = 78,
    /// @brief This note supersdes any ::SHORT_GUIRO or ::LONG_GUIRO notes playing
    OPEN_CUICA = 79,
    /// @brief This note supersdes any ::MUTE_TRIANGLE or ::OPEN_TRIANGLE notes playing
    MUTE_TRIANGLE = 80,
    /// @brief This note supersdes any ::MUTE_TRIANGLE or ::OPEN_TRIANGLE notes playing
    OPEN_TRIANGLE = 81,
    // Roland GS Extensions
    /*SHAKER = 82,
    JINGLE_BELL = 83,
    BELLTREE = 84,
    CASTANETS = 85,
    MUTE_SURDO = 86,
    OPEN_SURDO = 87,*/
    // End Roland GS Extensions
} percussionNote_t;

/**
 * @brief Defines the MIDI continuous controller registers
 *
 * Values suffixed with `_MSB` and `_LSB` represent the upper and lower 7 bits of two
 * separate MIDI controller values. Some values are designated as switches, which
 * will be considered off when set between 0 and 63, and on when set from 64 to 127.
 * All other values represent a single 7-bit value with a range of 0 to 127 inclusive
 * unless otherwise specified.
 */
typedef enum
{
    MCC_BANK_MSB             = 0,
    MCC_MODULATION_WHEEL_MSB = 1,
    MCC_BREATH_MSB           = 2,

    MCC_FOOT_PEDAL_MSB      = 4,
    MCC_PORTAMENTO_TIME_MSB = 5,
    MCC_DATA_ENTRY_MSB      = 6,
    MCC_VOLUME_MSB          = 7,
    MCC_BALANCE_MSB         = 8,

    MCC_PAN_MSB        = 10,
    MCC_EXPRESSION_MSB = 11,
    MCC_EFFECT_1_MSB   = 12,
    MCC_EFFECT_2_MSB   = 13,

    MCC_GP_SLIDER_1 = 16,
    MCC_GP_SLIDER_2 = 17,
    MCC_GP_SLIDER_3 = 18,
    MCC_GP_SLIDER_4 = 19,

    MCC_BANK_LSB             = 32,
    MCC_MODULATION_WHEEL_LSB = 33,
    MCC_BREATH_LSB           = 34,

    MCC_FOOT_PEDAL_LSB      = 36,
    MCC_PORTAMENTO_TIME_LSB = 37,
    MCC_DATA_ENTRY_LSB      = 38,
    MCC_VOLUME_LSB          = 39,
    MCC_BALANCE_LSB         = 40,

    MCC_PAN_LSB        = 42,
    MCC_EXPRESSION_LSB = 43,
    MCC_EFFECT_1_LSB   = 44,
    MCC_EFFECT_2_LSB   = 45,

    //< Switch
    MCC_HOLD_PEDAL = 64,
    //< Switch
    MCC_PORTAMENTO = 65,
    //< Switch
    MCC_SUSTENUTO_PEDAL = 66,
    //< Switch
    MCC_SOFT_PEDAL = 67,
    //< Switch
    MCC_LEGATO_PEDAL = 68,
    //< Switch
    MCC_HOLD_2_PEDAL       = 69,
    MCC_SOUND_VARIATION    = 70,
    MCC_SOUND_TIMBRE       = 71,
    MCC_SOUND_RELEASE_TIME = 72,
    MCC_SOUND_ATTACK_TIME  = 73,
    MCC_SOUND_BRIGHTNESS   = 74,
    // Decay
    MCC_SOUND_CONTROL_6 = 75,
    // Sustain level
    MCC_SOUND_CONTROL_7  = 76,
    MCC_SOUND_CONTROL_8  = 77,
    MCC_SOUND_CONTROL_9  = 78,
    MCC_SOUND_CONTROL_10 = 79,
    //< Switch
    MCC_GP_BUTTON_1 = 80,
    //< Switch
    MCC_GP_BUTTON_2 = 81,
    //< Switch
    MCC_GP_BUTTON_3 = 82,
    //< Switch
    MCC_GP_BUTTON_4 = 83,

    MCC_EFFECTS_LEVEL            = 91,
    MCC_TREMOLO_LEVEL            = 92,
    MCC_CHORUS_LEVEL             = 93,
    MCC_DETUNE_LEVEL             = 94,
    MCC_PHASER_LEVEL             = 95,
    MCC_DATA_BUTTON_INC          = 96,
    MCC_DATA_BUTTON_DEC          = 97,
    MCC_NON_REGISTERED_PARAM_LSB = 98,
    MCC_NON_REGISTERED_PARAM_MSB = 99,
    MCC_REGISTERED_PARAM_LSB     = 100,
    MCC_REGISTERED_PARAM_MSB     = 101,

    //< No data
    MCC_ALL_SOUND_OFF = 120,
    //< No data
    MCC_ALL_CONTROLS_OFF = 121,
    //< Switch
    MCC_LOCAL_KEYBOARD = 122,
    //< No data
    MCC_ALL_NOTE_OFF = 123,
    //< No data
    MCC_OMNI_MODE_OFF = 124,
    //< No data
    MCC_OMNI_MODE_ON = 125,
    //< No data
    MCC_MONO_OPERATION = 126,
    //< No data
    MCC_POLY_OPERATION = 127,
} midiControl_t;

//==============================================================================
// Structs
//==============================================================================

/**
 * @brief Describes the characteristics of a particular timbre while
 *
 */
typedef struct
{
    /// @brief Base time taken to ramp up to full volume
    int32_t attackTime;

    /// @brief This value will be multiplied by the note velocity and added to the attack time,
    q24_8 attackTimeVel;

    /// @brief Base time taken for the volume to fade to the sustain volume
    int32_t decayTime;

    /// @brief This value will be multiplied by the note velocity and added to the decay time
    q24_8 decayTimeVel;

    /// @brief Base time it takes to silence the note after release, in DAC samples
    int32_t releaseTime;

    /// @brief This value will be multiplied by the note velocity and added to the attack time
    q24_8 releaseTimeVel;

    /// @brief The base volume of the sustained note, proportional to the sample volume
    uint8_t sustainVol;

    /// @brief This value will be multiplied by the note velocity and added to the attack time
    q24_8 sustainVolVel;

    /// @brief The index of the repeat loop
    // uint32_t loopStart;
    /// @brief The index at which the loop should repeat
    // uint32_t loopEnd;

} envelope_t;

typedef struct
{
    /// @brief The number of chorused voices to mix
    uint8_t chorus;
} timbreEffects_t;

/**
 * @brief A function that returns samples for a percussion timbre rather than a melodic one
 *
 * @param drum The percussion instrument to generate sound for
 * @param idx The monotonic sample index within this note. Will not repeat for any particular note.
 * @param[out] done A pointer to a boolean to be set to 1 when the drum sample is finished playing
 * @param[in,out] scratch A pointer to an array of 4 uint32_t that will persist for the duration of the note
 * @param data A pointer to user-defined data which may be used in sample generation
 */
typedef int8_t (*percussionFunc_t)(percussionNote_t drum, uint32_t idx, bool* done, uint32_t scratch[4], void* data);

/**
 * @brief A function to handle text meta-messages from playing MIDI files
 *
 * @param type The type of meta-message
 * @param text The message text - NOT NUL-terminated
 * @param length The length of the text
 */
typedef void (*midiTextCallback_t)(metaEventType_t type, const char* text, uint32_t length);

/**
 * @brief A function to return MIDI events in streaming mode
 *
 */
typedef bool (*midiStreamingCallback_t)(midiEvent_t* event);

/**
 * @brief Defines the sound characteristics of a particular instrument.
 */
typedef struct
{
    /// @brief The source of samples for this instrument
    timbreType_t type;

    /// @brief Flags bitfield for this timbre
    timbreFlags_t flags;

    union
    {
        struct
        {
            /// @brief The index of this timbre's wave in the table, when type is WAVETABLE
            uint16_t waveIndex;

            /// @brief The function to use to retrieve wavetable samples
            waveFunc_t waveFunc;
        };

        struct
        {
            /// @brief The frequency of the base sample to be used when pitch shifting

            // This should just always be C4? (440 << 8)
            // uint32_t freq = (440 << 8);

            union
            {
                struct
                {
                    /// @brief A pointer to this timbre's sample data
                    const uint8_t* data;

                    /// @brief The length of the sample in bytes
                    uint32_t count;
                };

                struct
                {
                    /// @brief The name of the sample to load into data
                    const char* sampleName;
                } config;
            };

            /// @brief The sample rate.
            uint32_t rate;

            /// @brief The base frequency, at which the sample plays at normal speed
            uq8_24 baseNote;

            /// @brief 0 to loop forever, or the number of loops to play
            uint32_t loop;
        } sample;

        struct
        {
            /// @brief A callback to call for drum data
            percussionFunc_t playFunc;
            /// @brief User data to pass to the drumkit
            void* data;
        } percussion;

        /// @brief The shape of this wave, when type is RAW_WAVE
        oscillatorShape_t shape;
    };

    /// @brief The ASDR characterstics of this timbre
    envelope_t envelope;

    /// @brief Various effects applied to this timbre. May be ignored by percussion timbres
    timbreEffects_t effects;

    /// @brief The name of this timbre, if any
    const char* name;
} midiTimbre_t;

/**
 * @brief Tracks the state of a single voice, playing a single note.
 *
 * A single voice could use multiple oscillators to achieve a particular timbre.
 */
typedef struct
{
    /// @brief The number of samples remaining before transitioning to the next state
    uint32_t transitionTicks;

    /// @brief The number of samples remaining before the next volume adjustment
    uint32_t transitionTicksTotal;

    /// @brief The volume at the start of the transition time
    uint8_t transitionStartVol;

    /// @brief The target volume of this tick
    uint8_t targetVol;

    /// @brief The monotonic tick counter for playback of sampled timbres
    uint32_t sampleTick;

    /// @brief The MIDI note number for the sound being played
    uint8_t note;

    /// @brief The MIDI note velocity of the playing note
    uint8_t velocity;

    /// @brief The index of the MIDI channel that owns the currently playing note
    uint8_t channel;

    /// @brief The synthesizer oscillators used to generate the sounds
    synthOscillator_t oscillators[OSC_PER_VOICE];

    // TODO union this with the oscillators? They shouldn't both be used
    // But we need to make sure those oscillators don't get summed
    union
    {
        /// @brief An array of scratch data for percussion functions to use
        uint32_t percScratch[4];

        struct
        {
            /// @brief The number of fractional samples remaining
            uint32_t sampleError;

            /// @brief The number of loops remaining
            uint32_t sampleLoops;
        };
    };

    /// @brief A pointer to the timbre of this voice, which defines its musical characteristics
    const midiTimbre_t* timbre;
} midiVoice_t;

/**
 * @brief Holds several bitfields that track the state of each voice for fast access.
 * This may be used for dynamic voice allocation, and to minimize the impact of note stealing
 * if we run out of voices.
 */
typedef struct
{
    /// @brief Whether this note is set to on via MIDI, regardless of if it's making sound
    uint32_t on;

    /// @brief Bitfield of voices currently in the attack stage
    uint32_t attack;

    /// @brief Bitfield of voices currently in the sustain stage
    uint32_t sustain;

    /// @brief Bitfield of voices currently in the decay stage
    uint32_t decay;

    /// @brief Bitfield of voices currently in the release stage
    uint32_t release;

    /// @brief Bitfield of voices which are being held by the pedal
    uint32_t held;

    /// @brief Bitfield of voices which are being held by the sustenuto pedal
    uint32_t sustenuto;
} voiceStates_t;

/**
 * @brief Tracks the state of a single MIDI channel
 */
typedef struct
{
    /// @brief The 14-bit volume level for this channel only
    uint16_t volume;

    /// @brief The bank to use for program changes on this channel
    uint16_t bank;

    /// @brief The ID of the program (timbre) set for this channel
    uint8_t program;

    /// @brief The actual current timbre definition which the program ID corresponds to
    midiTimbre_t timbre;

    /// @brief A bitmap of which voices have been allocated to this channel
    uint32_t allocedVoices;

    /// @brief The 14-bit pitch wheel value
    uint16_t pitchBend;

    /// @brief Whether this channel is reserved for percussion.
    bool percussion;

    /// @brief Whether notes will be held after release
    bool held;

    /// @brief Whether certain notes will be held after release
    bool sustenuto;

    /// @brief If set, events on this channel will be completely ignored
    bool ignore;
} midiChannel_t;

/**
 * @brief Tracks the state of the entire MIDI apparatus.
 */
typedef struct
{
    /// @brief The global 14-bit volume level
    uint16_t volume;

    /// @brief The state of all MIDI channels
    midiChannel_t channels[MIDI_CHANNEL_COUNT];

    /// @brief The voices reserved for percussion
    midiVoice_t percVoices[PERCUSSION_VOICES];

    /// @brief The percussion voice state bitmaps
    voiceStates_t percVoiceStates;

    /// @brief A bitmap to track which percussion voices have special notes playing
    /// This includes all 3 hi-hats (open, closed, and pedal), short and long whistle and guiro,
    /// and mute and open cuica and triangle. This value contains 5 separate 6-bit fields, each
    /// of which contains either the index of the voice used by that instrument group (0 to 31)
    /// or has all 6 bits set to indicate that the instrument group has no voice allocated.
    uint32_t percSpecialStates;

    /// @brief The global voice pool for non-percussion channels
    midiVoice_t poolVoices[POOL_VOICE_COUNT];

    /// @brief The global voice pool state bitmaps
    voiceStates_t poolVoiceStates;

    /// @brief An array holding a pointer to every oscillator
    synthOscillator_t* allOscillators[(POOL_VOICE_COUNT + PERCUSSION_VOICES) * OSC_PER_VOICE];

    /// @brief The total number of oscillators in the \c allOscillators array
    uint16_t oscillatorCount;

    /// @brief Whether this player is playing a song or a MIDI stream
    midiPlayerMode_t mode;

    /// @brief A MIDI reader to use for file playback, when in MIDI_FILE mode
    midiFileReader_t reader;

    /// @brief A callback function to retrieve the next event, when in MIDI_STREAMING mode
    midiStreamingCallback_t streamingCallback;

    /// @brief A callback to call when a text meta-message is received
    midiTextCallback_t textMessageCallback;

    /// @brief A callback to call when the playing song is finished
    songFinishedCbFn songFinishedCallback;

    /// @brief The constant value to multiply each frame's samples by, before being shifted right 16 bits
    /// This value must be between 0x0000 (muted) and 0x7FFF (100% volume). The default value is 0x2666 (30%),
    /// and something around this value should be considered "full volume" with values higher than that being
    /// much more likely to clip for louder sounds
    int32_t headroom;

    /// @brief Number of samples that were clipped
    /// Note: This is not set when using \c midiPlayerFillBufferMulti()
    uint32_t clipped;

    /// @brief The number of samples elapsed in the playing song
    uint64_t sampleCount;

    /// @brief The next event in the MIDI file, which occurs after the current time
    midiEvent_t pendingEvent;

    /// @brief True if pendingEvent is valid, false if it must be updated
    bool eventAvailable;

    /// @brief The number of microseconds per quarter note
    uint32_t tempo;

    /// @brief True when playback of the current file is paused
    bool paused;

    /// @brief True when the MIDI player is seeking, and will not produce sound
    bool seeking;

    /// @brief If true, the playing file will automatically repeat when complete
    bool loop;
} midiPlayer_t;

/**
 * @brief Initialize the MIDI player
 *
 * @param player The MIDI player to initialize
 */
void midiPlayerInit(midiPlayer_t* player);

/**
 * @brief Reset the MIDI player state
 *
 * @param player The MIDI player to reset
 */
void midiPlayerReset(midiPlayer_t* player);

/**
 * @brief Calculate and return the next MIDI sample, stepping the player state forward by one sample
 *
 * @param player The player to step forward
 * @return int32_t The next signed 32-bit sample, without any headroom or clipping applied
 */
int32_t midiPlayerStep(midiPlayer_t* player);

/**
 * @brief Fill a buffer with the next set of samples from the MIDI player. This should be called by the
 * callback passed into initDac(). Samples are generated at sampling rate of ::DAC_SAMPLE_RATE_HZ
 *
 * @param player The MIDI player to sample from
 * @param samples An array of unsigned 8-bit samples to fill
 * @param len The length of the array to fill
 */
void midiPlayerFillBuffer(midiPlayer_t* player, uint8_t* samples, int16_t len);

/**
 * @brief Fill a buffer with the next set of samples from an array of MIDI players.
 *
 * @param players A pointer to an array of MIDI players
 * @param playerCount The number of MIDI players in the array
 * @param samples An array of unsigned 8-bit samples to fill
 * @param len The length of the array to fill
 */
void midiPlayerFillBufferMulti(midiPlayer_t* players, uint8_t playerCount, uint8_t* samples, int16_t len);

/**
 * @brief Stop all sound immediately. This is not affected by the sustain pedal.
 *
 * @param player The player to stop
 */
void midiAllSoundOff(midiPlayer_t* player);

/**
 * @brief Reset all controllers on a MIDI channel.
 *
 * This includes:
 * - Sustain Pedal
 * - Legato, Vibrato, Sustenuto, etc.
 *
 * @param player
 * @param channel
 */
void midiResetChannelControllers(midiPlayer_t* player, uint8_t channel);

/**
 * @brief Activate General MIDI mode on a MIDI player
 *
 * @param player The MIDI player to set to General MIDI mode
 */
void midiGmOn(midiPlayer_t* player);

/**
 * @brief Deactivate General MIDI mode on a MIDI player
 *
 * @param player The MIDI player to take out of General MIDI mode
 */
void midiGmOff(midiPlayer_t* player);

/**
 * @brief Tun off all notes which are currently on, as though midiNoteOff() were called
 * for each note. This respects the sustain pedal.
 *
 * @param player The MIDI player
 * @param channel The MIDI channel on which to stop notes
 */
void midiAllNotesOff(midiPlayer_t* player, uint8_t channel);

/**
 * @brief Begin playing a note on a given MIDI channel.
 *
 * Using a velocity of \c 0 is equivalent to calling midiNoteOff()
 *
 * @param player The MIDI player
 * @param channel The MIDI channel on which to start the note
 * @param note The note number to play
 * @param velocity The note velocity which affects its volume and effects
 */
void midiNoteOn(midiPlayer_t* player, uint8_t channel, uint8_t note, uint8_t velocity);

/**
 * @brief Change the velocity of a note on a given MIDI channel, after the note starts playing
 *
 * @param player The MIDI player
 * @param channel The MIDI channel on which to change the note velocity
 * @param note The currently-playing note number to modify
 * @param velocity The note velocity which affects its volume and effects
 */
void midiAfterTouch(midiPlayer_t* player, uint8_t channel, uint8_t note, uint8_t velocity);

/**
 * @brief Stop playing a particular note on a given MIDI channel
 *
 * @param player The MIDI player
 * @param channel The MIDI channel on which to stop the note
 * @param note The note number to stop
 * @param velocity [NYI] The release velocity which affects the note's release time
 */
void midiNoteOff(midiPlayer_t* player, uint8_t channel, uint8_t note, uint8_t velocity);

/**
 * @brief Change the program (instrument) on a given MIDI channel
 *
 * @param player The MIDI player
 * @param channel The MIDI channel whose program will be changed
 * @param program The program ID, from 0-127, to set for this channel
 */
void midiSetProgram(midiPlayer_t* player, uint8_t channel, uint8_t program);

/**
 * @brief Set the hold pedal status.
 *
 * When set, all notes that are currently on will be sustained until the pedal is unset, as well as
 * all notes that are played after the pedal is set.
 *
 * This is a convenience method for midiControlChange(player, channel, MCC_HOLD_PEDAL (64), val ? 127:0)
 *
 * @param player The MIDI player
 * @param channel The MIDI channel to set the hold status for
 * @param val The sustain pedal value. Values 0-63 are OFF, and 64-127 are ON.
 */
void midiSustain(midiPlayer_t* player, uint8_t channel, uint8_t val);

/**
 * @brief Set the sustenuto pedal status.
 *
 * When set, only the notes that are currently on will be sustained until the pedal is unset. Notes
 * that are played after the pedal is set will not be sustained.
 *
 * @param player The MIDI player
 * @param channel The MIDI channel to set the sustenuto status for
 * @param val The sustenuto pedal value. Values 0-63 are OFF, and 64-127 are ON.
 */
void midiSustenuto(midiPlayer_t* player, uint8_t channel, uint8_t val);

/**
 * @brief Set a MIDI control value
 *
 * @param player The MIDI player
 * @param channel The channel to set the control on
 * @param control The control number to set
 * @param val The control value, from 0-127 whose meaning depends on the control number
 */
void midiControlChange(midiPlayer_t* player, uint8_t channel, midiControl_t control, uint8_t val);

/**
 * @brief Get the value of a MIDI control
 *
 * @param player The MIDI player
 * @param channel The channel to retrieve the control from
 * @param control The MIDI control number of the control
 * @return uint8_t The value of the specified control, or 0 if the specified controller is not implemented
 */
uint8_t midiGetControlValue(midiPlayer_t* player, uint8_t channel, midiControl_t control);

/**
 * @brief Get the combined value of two MIDI control registers
 *
 * @param player The MIDI player
 * @param channel The channel to retrieve the control from
 * @param control The MIDI control number of either of the two controls that make up the value
 * @return uint16_t The value of the specified control, or 0 if the specified controller is not implemented
 */
uint16_t midiGetControlValue14bit(midiPlayer_t* player, uint8_t channel, midiControl_t control);

/**
 * @brief Set the pitch wheel value on a given MIDI channel
 *
 * By default, the center of the pitch wheel is \c 0x2000. A value of \c 0x0000 transposes
 * one step down, while a value of \c 0x3FFF transposes one step down.
 *
 * [NYI] The range of the pitch wheel can be changed using the registered parameters, with
 * MSB being the range in (+/-)semitones and LSB being the range in (+/-) cents
 *
 * @param player The MIDI player
 * @param channel The MIDI channel to change the pitch wheel for
 * @param value The pitch wheel value, from 0 to 0x3FFF (14-bits)
 */
void midiPitchWheel(midiPlayer_t* player, uint8_t channel, uint16_t value);

/**
 * @brief Change the MIDI playback tempo
 *
 * @param player The player to change the tempo of
 * @param tempo The new tempo to set, in microseconds per quarter note
 */
void midiSetTempo(midiPlayer_t* player, uint32_t tempo);

/**
 * @brief Configure this MIDI player to read from a MIDI file
 *
 * @param player The MIDI player
 * @param file A pointer to the MIDI file to be played
 */
void midiSetFile(midiPlayer_t* player, const midiFile_t* file);

/**
 * @brief Set the paused state of a MIDI song
 *
 * @param player The player
 * @param pause True to pause, false to play
 */
void midiPause(midiPlayer_t* player, bool pause);

/**
 * @brief Seek to a given time offset within a file
 *
 * Note that in the current implementation, seeking backwards by any amount requires
 * re-reading the file from the beginning, and so may be very slow, particularly for
 * large MIDI files.
 *
 * @param player The MIDI player to seek on
 * @param ticks The absolute number of MIDI ticks to seek to.
 */
void midiSeek(midiPlayer_t* player, uint32_t ticks);

//==============================================================================
// Global MIDI Player Functions
//==============================================================================

/**
 * @brief Initialize the system-wide MIDI players for both BGM and SFX
 */
void initGlobalMidiPlayer(void);

/**
 * @brief Deinitialize and free memory associated with the system-wide MIDI players
 *
 */
void deinitGlobalMidiPlayer(void);

/**
 * @brief Fill the given sample buffer with samples from both the BGM and SFX system-wide
 * MIDI players
 *
 * @param[out] samples A pointer to the array of unsigned 8-bit samples to be filled
 * @param len The number of samples
 */
void globalMidiPlayerFillBuffer(uint8_t* samples, int16_t len);

/**
 * @brief Play a song on one of the system-wide MIDI players, either BGM or SFX
 *
 * @param song A pointer to the song to play
 * @param trackType The player to use, either MIDI_SFX or MIDI_BGM
 */
void globalMidiPlayerPlaySong(midiFile_t* song, uint8_t trackType);

/**
 * @brief Play a song on noe of the system-wide MIDI players, with a callback once the song finishes
 *
 * @param song A pointer to the song to play
 * @param trackType The player to use, either MIDI_SFX or MIDI_BGM
 * @param cb The function to be called once the song completes
 */
void globalMidiPlayerPlaySongCb(midiFile_t* song, uint8_t trackType, songFinishedCbFn cb);

/**
 * @brief Set the volume using a value from 0 to 13
 *
 * @param trackType The player to set the volume for, either MIDI_SFX or MIDI_BGM
 * @param volumeSetting The volume value
 */
void globalMidiPlayerSetVolume(uint8_t trackType, int32_t volumeSetting);

/**
 * @brief Pause all songs currently being played by the system-wide MIDI players
 *
 */
void globalMidiPlayerPauseAll(void);

/**
 * @brief Resume all songs currently being played by the system-widi MIDI players
 *
 */
void globalMidiPlayerResumeAll(void);

/**
 * @brief Stop all songs currently bein gplayed by the system-widi MIDI players,
 * optionally resetting their state to the beginning of the song
 *
 * @param reset if true, the players will be reset to the beginning of the song
 */
void globalMidiPlayerStop(bool reset);

/**
 * @brief Stop all MIDI playback and return a pointer containing the full playback state.
 * This state must be passed to globalMidiRestore() later to restore the previous state.
 *
 * @return void* A void-pointer containing the full MIDI playback state
 */
void* globalMidiSave(void);

/**
 * @brief Resume MIDI playback from the state stored in the given pointer. The data will
 * be freed after this call and cannot be reused.
 *
 * @param data The playback state data returned by globalMidiSave()
 */
void globalMidiRestore(void* data);

/**
 * @brief Return a pointer to the system-wide MIDI player for the given track type,
 * either MIDI_SFX or MIDI_BGM
 *
 * @param trackType Which MIDI player to return, either MIDI_SFX or MIDI_BGM
 * @return midiPlayer_t*
 */
midiPlayer_t* globalMidiPlayerGet(uint8_t trackType);
