//Author: Levi Amendt levi.amendt@gmail.com
#define CHAR_MAX 127
#define CHAR_MIN -128

#define INPUT_BUF_SIZE 128
#define MAX_DECK_SIZE 64
#define NUM_STATS 4
#define NUM_TYPES 5
#define NUM_CONDITIONS 4
#define MAX_EFFECTS 3
#define POISON_VALUE 2
#define CARDS_PER_TURN 3
#define NUM_FLOORS 10
#define ENEMY_DECK_SIZE 5
#define ENEMY_XP_PER_FLOOR 7
#define ENEMY_HEALTH_PER_FLOOR 8
#define START_XP 20
#define PLAYER_HEALTH 20
#define DECK_START_SIZE 5
#define REST_HEALTH_INCREASE 10
#define POISON_DECAY 3
#define ENEMY_START_XP 10
#define ENEMY_START_HP 10
#define XP_PER_FIGHT 3

char typeValues[] = {1, 5, 3, 5, 6};
char statValues[] = {2, 3, 3, 4};
char* typeString = {"\0Debuff \0Buff \0Rpt \0Rebuff "};
char* statString = {"Dmg \0HP \0Shld \0Poison "};
char* conditionString = {"\0Hi HP\n\0Lo HP\n\0Rnd\n"};
char NL = '\n';

typedef enum : char {
    NORMAL,
    DEBUFF,
    BUFF,
    REPEAT,
    REBUFF,
} Type;

typedef enum : char {
    DAMAGE,
    HEALTH,
    SHEILD,
    POISON,
} Stat;

typedef enum : char {
    NONE,
    GT_HEALTH,
    LT_HEALTH,
    RANDOM,
} Condition;

typedef enum : char {
    SOURCE = 1,
    TARGET,
} TurnDeath;

typedef struct {
    char numEffects;
    Condition condition;
    Type types[MAX_EFFECTS];
    Stat stats[MAX_EFFECTS];
    char values[MAX_EFFECTS];
} Card;

typedef struct {
    char size;
    char index;
    Card cards[MAX_DECK_SIZE];
} Deck;

typedef struct {
    char maxHealth;
    char xp;
    char stats[NUM_STATS];
    char buffs[NUM_STATS];
    char repeats[NUM_STATS];
    char rebuffs[NUM_STATS];
    Deck deck;
} Entity;

static void printn(char num);
static void prints(char* str);
static void printnl();
static void write(char* str, char len);
static void read();
static void exit();
static void seed();
static unsigned int rand();
static void useCard(Card *card, Entity *source, Entity *target);
static void fight();
static void dealDamage(short damage, Entity* target);
static void shuffle(Deck* deck);
static void generateCard(Card* card, char xp);
static void death();
static char turn(Entity *source, Entity *target);
static void initStats(Entity *entity);
static void generateEnemy();
static char* stringI(char* str, char index);
static void printCard(Card *card);
static void addCard(Card* card, Deck* deck);
static void restOrPurge();
static void removeCard(Deck* deck, char index);
static void generateDeck(Deck* deck, char size, char xp);
static void printDeck(Deck* deck);
static void printBattleState();
static void printBattleHelper(Entity* entity);
static void modifyStat(char* stat, short value);

//globals
unsigned int rngState;
Entity player, enemy;
char floor;
char inputBuf[INPUT_BUF_SIZE];

__attribute__((section(".text.startup"), noreturn))
void _start(void) {
    seed();
    player.maxHealth = PLAYER_HEALTH;
    player.stats[HEALTH] = PLAYER_HEALTH;
    player.xp = START_XP;
    generateDeck(&player.deck, DECK_START_SIZE, START_XP);
    prints("\nWelcome! \n\nDeck:\n\n");
    printDeck(&player.deck);
    for (floor = 0; floor < NUM_FLOORS; floor++) {
        fight();
        fight();
        restOrPurge();
        prints("\nFloor ");
        printn(floor);
        prints(" done!\n");
        read();
    }
    prints("Victory!");
    exit();
    __builtin_unreachable();
}

void write(char* str, char len) {
    long sys;
    asm volatile("syscall": "=a"(sys): "a"(1), "D"(1), "S"(str), "d"((long)len): "rcx", "r11", "memory");
    return;
}

void printnl() {
    write(&NL,1);
}

void read() {
    long n;
    asm volatile("syscall": "=a"(n): "a"(0), "D"(0), "S"(inputBuf), "d"(INPUT_BUF_SIZE): "rcx", "r11", "memory");
    inputBuf[n] = '\0';
    return;
}

void exit() {
    asm volatile("syscall": : "a"(60), "D"(0): "rcx", "r11");
    return;
}

void prints(char* str) {
    char len = 0;
    for (; str[len] != '\0'; len++);
    write(str, len);
    return;
}

void printn(char num) {
    if (num < 0) {
        prints("-");
        num = -num;
    }
    char i = 11;
    char str[12];
    do {
        str[i--] = num % 10 + '0';
        num /= 10;
    } while (num);
    write(str + i + 1, 11 - i);
    return;
}


void seed(void) {
    unsigned int lo, hi;
    asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
    rngState = lo ^ hi | 1;
    return;
}

unsigned int rand(void) {
    rngState ^= rngState << 13;
    rngState ^= rngState >> 17;
    rngState ^= rngState << 5;
    return rngState;
}

void useCard(Card *card, Entity *source, Entity *target) {
    printCard(card);
    char loHealth = source->maxHealth / source->stats[HEALTH] > 1;
    switch (card->condition) {
    case GT_HEALTH:
        if (loHealth){
            conFail:
            prints("Fail\n");
            return;
        }
        break;
    case LT_HEALTH:
        if (!loHealth) {
            goto conFail;
        }
        break;
    case RANDOM:
        if (rand() % 2) {
            goto conFail;
        }
        break;
    default:
        break;
    }
    for (char i = 0; i < card->numEffects; i++) {
        switch (card->types[i]) {
        case NORMAL:
            char b = source->buffs[card->stats[i]];
            switch (card->stats[i]) {
            case DAMAGE:
                dealDamage(card->values[i] + b, target);
                break;
            case POISON:
                modifyStat(&target->stats[card->stats[i]], b + card->values[i]);
                break;
            default:
                modifyStat(&source->stats[card->stats[i]], b + card->values[i]);
                if (source->stats[HEALTH] > source->maxHealth)
                    source->stats[HEALTH] = source->maxHealth;
                break;
            }
            break;
        case DEBUFF:
            modifyStat(&target->buffs[card->stats[i]], -card->values[i]);
            break;
        default:
            modifyStat(&source->stats[(card->types[i] - 1) * NUM_STATS + card->stats[i]], card->values[i]);
            break;
        }
    }
    return;
}

void modifyStat(char* stat, short value) {
    if (__builtin_add_overflow(*stat, value, stat)) {
        if (value > 0)
            *stat = CHAR_MAX;
        else
            *stat = CHAR_MIN;
    }
}

char turn(Entity *source, Entity *target) {
    source->stats[HEALTH] -= source->stats[POISON];
    source->stats[POISON] -= POISON_DECAY;
    if (source->stats[POISON] < 0) source->stats[POISON] = 0;
    if (source->stats[HEALTH] < 1)
        return SOURCE;
    for (Stat s = HEALTH; s <= SHEILD; s++) 
        modifyStat(&source->stats[s], source->repeats[s]);
    if (source->stats[HEALTH] > source->maxHealth)
        source->stats[HEALTH] = source->maxHealth;
    dealDamage(source->repeats[DAMAGE], target);
    if (target->stats[HEALTH] < 1)
        return TARGET;
    modifyStat(&target->stats[POISON], source->repeats[POISON]);
    for (char i = 0; i < NUM_STATS; i++) {
        modifyStat(&source->buffs[i], source->rebuffs[i]);
    }
    
    for (char i = 0; i < CARDS_PER_TURN; i++) {
        if (++source->deck.index == source->deck.size)
            shuffle(&source->deck);
        useCard(&source->deck.cards[source->deck.index], source, target);
        printBattleState();
        read();
        if (target->stats[HEALTH] < 1)
            return TARGET;
    }
    return NONE;
}

void fight() {
    char pHealth = player.stats[HEALTH];
    initStats(&player);
    player.stats[HEALTH] = pHealth;
    shuffle(&player.deck);
    generateEnemy();
    while(1) {
        prints("Your turn\n\n");
        char ret = turn(&player, &enemy);
        if (ret == SOURCE)
            death();
        else if (ret == TARGET) {
            break;
        }
        prints("Enemy turn\n\n");
        ret = turn(&enemy, &player);
        if (ret == TARGET)
            death();
        else if (ret == SOURCE)
            break;
        
    }
    prints("\nWin! XP: ");
    player.xp += XP_PER_FIGHT;
    printn(player.xp);
    printnl();
    Card choices[3];
    for (char i = 0; i < 3; i++) {
        generateCard(&choices[i], player.xp);
        prints("\nCard ");
        printn(i+1);
        prints(":\n");
        printCard(&choices[i]);
    }
    printnl();
    prompt:
    prints("[1-3] Skip [0]: ");
    read();
    if (inputBuf[0] == '0') return;
    else if (inputBuf[0] >= '0' && inputBuf[0] <= '3')
        addCard(&choices[inputBuf[0] - '0' - 1], &player.deck);
    else
        goto prompt;
    return;
}

void generateCard(Card* card, char xp) {
    short budget = (rand() % (xp / 2)) + (xp / 2);
    card->numEffects = rand() % (MAX_EFFECTS + 2) + 1;
    if (card->numEffects > MAX_EFFECTS)
        card->numEffects = 1;
    card->condition = rand() % (NUM_CONDITIONS * 3);
    if (card->condition > NUM_CONDITIONS - 1 || card->condition == NONE)
        card->condition = NONE;
    else
        budget *= 2;
    for (char i = 0; i < card->numEffects; i++) {
        short x = rand() % budget;
        budget -= x;
        card->types[i] = rand() % (NUM_TYPES * 2);
        if (card->types[i] > NUM_TYPES - 1)
            card->types[i] = NORMAL;
        card->stats[i] = rand() % (NUM_STATS);
        x /= typeValues[card->types[i]];
        x /= statValues[card->stats[i]];
        card->values[i] = x + 1;
    }
    return;
}

void generateEnemy() {  
    initStats(&enemy);
    enemy.maxHealth = floor * ENEMY_HEALTH_PER_FLOOR + ENEMY_START_HP;
    enemy.stats[HEALTH] = enemy.maxHealth;
    enemy.xp = floor * ENEMY_XP_PER_FLOOR + ENEMY_START_XP;
    generateDeck(&enemy.deck, ENEMY_DECK_SIZE, enemy.xp);
    return;
}

void generateDeck(Deck* deck, char size, char xp) {
    deck->size = size;
    deck->index = 0;
    for (char i = 0; i < size; i++) {
        generateCard(&deck->cards[i], xp);
    }
    return;
}

void shuffle(Deck *deck) {
    prints("Shuffle\n\n");
    for (char i = 0; i < deck->size - 1; i++) {
        char j = rand() % (deck->size - i) + i;
        Card tmp = deck->cards[i];
        deck->cards[i] = deck->cards[j];
        deck->cards[j] = tmp;
    }
    deck->index = 0;
    return;
}

void dealDamage(short damage, Entity* target) {
    modifyStat(&target->stats[SHEILD], -damage);
    short s = target->stats[SHEILD];
    if (s < 0) {
        target->stats[SHEILD] = 0;
        target->stats[HEALTH] += s;
    }
    return;
}

void death() {
    prints("Dead on floor ");
    printn(floor);
    printnl();
    exit();
    return;
}

void initStats(Entity *entity) {
    for (char i = 0; i < NUM_STATS * 4; i++) {
        entity->stats[i] = 0;
    }
    return;
}

void printCard(Card* card) {
    prints(stringI(conditionString, card->condition));
    for (char i = 0; i < card->numEffects; i++) {
        prints(stringI(typeString, card->types[i]));
        prints(stringI(statString, card->stats[i]));
        printn(card->values[i]);
        printnl();
    }
    return;
}

void printDeck(Deck* deck) {
    for (char i = 0; i < deck->size; i++) {
        prints("Card ");
        printn(i+1);
        prints(": \n");
        printCard(&deck->cards[i]);
        read();
    }
}

char* stringI(char* str, char index) {
    char j = 0;
    for (char i = 0; i < index; i++) {
        while (str[j] != '\0')
            j++;
        j++;
    }
    return str + j;
}

void addCard(Card *card, Deck *deck) {
    deck->cards[deck->size++] = *card;
    return;
}

void removeCard(Deck *deck, char index) {
    deck->cards[index] = deck->cards[--deck->size]; 
    return;
}

void restOrPurge() {
    prompt:
    prints("Rest [r] purge [p]: ");
    read();

    if (inputBuf[0] == 'r') {
        player.stats[HEALTH] = player.maxHealth += REST_HEALTH_INCREASE;
    }
    else if (inputBuf[0] == 'p') {
        char purgeIndex = 0;
        prints("\nNext [] purge [p]: \n");
        read();
        prompt2:
        printCard(&player.deck.cards[++purgeIndex % player.deck.size]);
        read();
        if (inputBuf[0] == 'p')
            removeCard(&player.deck, purgeIndex % player.deck.size);
        else goto prompt2;
    }
    else goto prompt;
    return;
}

void printBattleState() {
    prints("\nYou: ");
    printBattleHelper(&player);
    printnl();
    prints("Enemy: ");
    printBattleHelper(&enemy);
}

void printBattleHelper(Entity* entity) {
    prints("\t\tStat\t\tBuff\t\tRpt\t\tRebuff\n");
    for (char stat = 0; stat < NUM_STATS; stat++) {
        prints(stringI(statString, stat));
        for (char cat = 0; cat < 4; cat++) {
            prints("\t\t");
            printn(entity->stats[cat*4+stat]);
        }
        printnl();
    }
}