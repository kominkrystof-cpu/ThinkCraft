#include "thinkcraft/survival.h"

#include <math.h>
#include <stdio.h>

static int check(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "test_survival: %s\n", message);
    }
    return condition;
}

static int nearly(float value, float expected) {
    return fabsf(value - expected) < 0.001f;
}

int main(void) {
    tc_survival survival;
    int ok = 1;

    tc_survival_init(&survival);
    ok = ok && check(nearly(survival.health, 20.0f),
                     "initial health is wrong")
      && check(nearly(survival.hunger, 20.0f),
               "initial hunger is wrong")
      && check(survival.alive, "player did not start alive");

    ok = ok && check(nearly(tc_survival_damage(&survival, 5.0f), 5.0f),
                     "damage was not applied")
      && check(nearly(survival.health, 15.0f), "health after damage is wrong")
      && check(nearly(tc_survival_heal(&survival, 2.0f), 2.0f),
               "heal was not applied")
      && check(nearly(survival.health, 17.0f), "health after heal is wrong")
      && check(nearly(tc_survival_heal(&survival, 100.0f), 3.0f),
               "healing was not capped")
      && check(nearly(survival.health, 20.0f), "healing exceeded max health");

    ok = ok && check(nearly(tc_survival_damage(&survival, 100.0f), 20.0f),
                     "lethal damage was not clamped")
      && check(nearly(survival.health, 0.0f), "lethal damage left health")
      && check(!survival.alive, "lethal damage did not kill the player")
      && check(nearly(tc_survival_damage(&survival, 1.0f), 0.0f),
               "dead player took more damage");

    tc_survival_init(&survival);
    ok = ok && check(nearly(tc_survival_apply_fall(&survival, 2.9f), 0.0f),
                     "short fall caused damage")
      && check(nearly(survival.health, 20.0f),
               "short fall changed health")
      && check(nearly(tc_survival_apply_fall(&survival, 4.9f), 1.0f),
               "fall damage was not calculated")
      && check(nearly(survival.health, 19.0f), "fall damage was not applied")
      && check(nearly(tc_survival_apply_fall(&survival, 7.0f), 4.0f),
               "large fall damage was not calculated");

    tc_survival_init(&survival);
    tc_survival_add_exhaustion(&survival, 9.0f);
    tc_survival_tick(&survival, 0.016f);
    ok = ok && check(nearly(survival.hunger, 18.0f),
                     "exhaustion did not drain hunger")
      && check(nearly(survival.exhaustion, 1.0f),
               "exhaustion remainder is wrong");

    tc_survival_init(&survival);
    survival.health = 10.0f;
    tc_survival_tick(&survival, TC_SURVIVAL_REGENERATION_INTERVAL);
    ok = ok && check(nearly(survival.health, 11.0f),
                     "regeneration did not heal")
      && check(nearly(survival.hunger, 19.0f),
               "regeneration did not consume exhaustion");

    tc_survival_init(&survival);
    survival.health = 5.0f;
    survival.hunger = 0.0f;
    tc_survival_tick(&survival, TC_SURVIVAL_STARVATION_INTERVAL);
    ok = ok && check(nearly(survival.health, 4.0f),
                     "starvation did not damage")
      && check(survival.alive, "starvation killed too early");

    survival.health = 1.0f;
    tc_survival_tick(&survival, TC_SURVIVAL_STARVATION_INTERVAL);
    ok = ok && check(nearly(survival.health, 0.0f)
                     && !survival.alive,
                     "starvation did not kill the player");

    tc_survival_init(&survival);
    survival.health = 0.0f;
    survival.alive = 0;
    survival.hunger = 0.0f;
    tc_survival_tick(&survival, 10.0f);
    ok = ok && check(nearly(survival.health, 0.0f),
                     "dead player was healed by tick");

    if (!ok) {
        return 1;
    }

    printf("test_survival: all checks passed\n");
    return 0;
}
