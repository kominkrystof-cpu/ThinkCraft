#include "thinkcraft/survival.h"

#include <math.h>
#include <stddef.h>

void tc_survival_init(tc_survival *survival) {
    if (survival == NULL) {
        return;
    }

    survival->health = TC_SURVIVAL_MAX_HEALTH;
    survival->hunger = TC_SURVIVAL_MAX_HUNGER;
    survival->exhaustion = 0.0f;
    survival->regeneration_timer = 0.0f;
    survival->starvation_timer = 0.0f;
    survival->fall_distance = 0.0f;
    survival->damage_flash = 0.0f;
    survival->alive = 1;
}

void tc_survival_reset(tc_survival *survival) {
    tc_survival_init(survival);
}

float tc_survival_damage(tc_survival *survival, float amount) {
    float applied;

    if (survival == NULL || amount <= 0.0f || !survival->alive) {
        return 0.0f;
    }

    applied = fminf(amount, survival->health);
    survival->health -= applied;
    if (survival->health <= 0.0f) {
        survival->health = 0.0f;
        survival->alive = 0;
    }
    survival->damage_flash = TC_SURVIVAL_DAMAGE_FLASH_TIME;

    return applied;
}

float tc_survival_heal(tc_survival *survival, float amount) {
    float applied;

    if (survival == NULL || amount <= 0.0f || !survival->alive) {
        return 0.0f;
    }

    applied = fminf(amount, TC_SURVIVAL_MAX_HEALTH - survival->health);
    survival->health += applied;
    if (survival->health > TC_SURVIVAL_MAX_HEALTH) {
        survival->health = TC_SURVIVAL_MAX_HEALTH;
    }

    return applied;
}

void tc_survival_add_exhaustion(tc_survival *survival, float amount) {
    if (survival == NULL || amount <= 0.0f || !survival->alive) {
        return;
    }

    survival->exhaustion += amount;
    if (survival->exhaustion > 100.0f) {
        survival->exhaustion = 100.0f;
    }
}

float tc_survival_apply_fall(tc_survival *survival, float distance) {
    float damage;

    if (survival == NULL || distance <= TC_SURVIVAL_SAFE_FALL_DISTANCE) {
        return 0.0f;
    }

    damage = floorf(distance - TC_SURVIVAL_SAFE_FALL_DISTANCE);
    if (damage <= 0.0f) {
        return 0.0f;
    }

    return tc_survival_damage(survival, damage);
}

void tc_survival_tick(tc_survival *survival, float dt) {
    if (survival == NULL || dt <= 0.0f) {
        return;
    }

    if (survival->damage_flash > 0.0f) {
        survival->damage_flash -= dt;
        if (survival->damage_flash < 0.0f) {
            survival->damage_flash = 0.0f;
        }
    }

    if (!survival->alive) {
        return;
    }

    while (survival->exhaustion >= TC_SURVIVAL_EXHAUSTION_PER_HUNGER
           && survival->hunger > 0.0f) {
        survival->exhaustion -= TC_SURVIVAL_EXHAUSTION_PER_HUNGER;
        survival->hunger -= 1.0f;
    }
    if (survival->hunger < 0.0f) {
        survival->hunger = 0.0f;
    }

    if (survival->hunger >= TC_SURVIVAL_REGEN_MIN_HUNGER
        && survival->health < TC_SURVIVAL_MAX_HEALTH) {
        survival->regeneration_timer += dt;
        while (survival->regeneration_timer
               >= TC_SURVIVAL_REGENERATION_INTERVAL) {
            survival->regeneration_timer -= TC_SURVIVAL_REGENERATION_INTERVAL;
            tc_survival_heal(survival, 1.0f);
            tc_survival_add_exhaustion(survival,
                                       TC_SURVIVAL_REGEN_EXHAUSTION);
            if (!survival->alive) {
                return;
            }
        }
    } else {
        survival->regeneration_timer = 0.0f;
    }

    if (survival->hunger <= 0.0f) {
        survival->starvation_timer += dt;
        while (survival->starvation_timer
               >= TC_SURVIVAL_STARVATION_INTERVAL) {
            survival->starvation_timer -= TC_SURVIVAL_STARVATION_INTERVAL;
            tc_survival_damage(survival, 1.0f);
            if (!survival->alive) {
                return;
            }
        }
    } else {
        survival->starvation_timer = 0.0f;
    }

    while (survival->exhaustion >= TC_SURVIVAL_EXHAUSTION_PER_HUNGER
           && survival->hunger > 0.0f) {
        survival->exhaustion -= TC_SURVIVAL_EXHAUSTION_PER_HUNGER;
        survival->hunger -= 1.0f;
    }
}
