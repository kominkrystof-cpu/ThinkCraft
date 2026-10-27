#ifndef THINKCRAFT_SURVIVAL_H
#define THINKCRAFT_SURVIVAL_H

#define TC_SURVIVAL_MAX_HEALTH 20.0f
#define TC_SURVIVAL_MAX_HUNGER 20.0f
#define TC_SURVIVAL_EXHAUSTION_PER_HUNGER 4.0f
#define TC_SURVIVAL_SAFE_FALL_DISTANCE 3.0f
#define TC_SURVIVAL_REGENERATION_INTERVAL 4.0f
#define TC_SURVIVAL_STARVATION_INTERVAL 4.0f
#define TC_SURVIVAL_REGEN_MIN_HUNGER 18.0f
#define TC_SURVIVAL_REGEN_EXHAUSTION 6.0f
#define TC_SURVIVAL_DAMAGE_FLASH_TIME 0.35f

typedef struct {
    float health;
    float hunger;
    float exhaustion;
    float regeneration_timer;
    float starvation_timer;
    float fall_distance;
    float damage_flash;
    int alive;
} tc_survival;

void tc_survival_init(tc_survival *survival);
void tc_survival_reset(tc_survival *survival);
float tc_survival_damage(tc_survival *survival, float amount);
float tc_survival_heal(tc_survival *survival, float amount);
void tc_survival_add_exhaustion(tc_survival *survival, float amount);
float tc_survival_apply_fall(tc_survival *survival, float distance);
void tc_survival_tick(tc_survival *survival, float dt);

#endif
