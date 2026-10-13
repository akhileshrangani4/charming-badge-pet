#pragma once
namespace pet { namespace cfg {
constexpr float FOOD_EMPTY_MIN = 240, JOY_EMPTY_MIN = 300, HYGIENE_EMPTY_MIN = 480;
constexpr float ENERGY_EMPTY_MIN = 360, ENERGY_REFILL_MIN = 240;
constexpr float SLEEP_DECAY = 0.25f, SICK_DECAY = 2.0f;
constexpr int   MESS_AFTER_MEAL_MIN = 60;  constexpr float MESS_HYGIENE_HIT = 15;
constexpr int   SICK_ZERO_MIN = 60, SICK_MESS_MIN = 60, SICK_STAGE_UP_MIN = 180, MAX_SICK = 3;
constexpr int   BELLYACHE_SNACKS = 3, BELLYACHE_WINDOW_MIN = 30;
constexpr float TANTRUM_PER_MIN = 1.0f / 120, TANTRUM_OK_STAT = 30;
constexpr int   TANTRUM_IGNORE_MIN = 2, CALM_WINDOW_MIN = 10;
constexpr int   RUNAWAY_SICK_NOCARE_MIN = 480, RUNAWAY_ALLZERO_MIN = 240;
constexpr int   WINBACK_SESSIONS = 3, WINBACK_SESSIONS_LONG = 5, WINBACK_LONG_GONE_MIN = 2880, WINBACK_GAP_MIN = 60;
constexpr float RETURN_STAT = 40, RETURN_TRUST_CAP = 20, WON_BACK_TRUST = 35;
constexpr int   NIGHT_START_H = 23, NIGHT_END_H = 7;
constexpr float NIGHT_WAKE_JOY = 10;
constexpr float PAUSE_BUDGET_MIN = 600, PAUSE_REFILL_PER_MIN = 600.0f / 1440;
constexpr int   GROWTH_DAYS[3] = {3, 7, 14};  constexpr float GROWTH_TRUST = 50;
constexpr float MEAL_FOOD = 40, SNACK_FOOD = 10, SNACK_JOY = 10;
constexpr float PLAY_WIN_JOY = 25, PLAY_LOSE_JOY = 10, PLAY_ENERGY = 10;
constexpr float STROKE_JOY = 5, SHAKE_JOY = 5, SHAKE_TRUST = 2, NEEDLESS_MED_JOY = 5;
constexpr int   STROKE_COOLDOWN_MIN = 2;
constexpr float NEED_THRESHOLD = 30, TRUST_NEED_GAIN = 2, TRUST_CARE_GAIN = 0.5f;
constexpr float SPOIL_STEP = 10, DISCIPLINE_LOSS = 5, DISCIPLINE_GAIN = 10;
constexpr int   MAX_CATCHUP_MIN = 30 * 1440;
constexpr int   COFFEE_COOLDOWN_MIN = 180;   // COFFEE refills energy, once every 3 hours
}}
