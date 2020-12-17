/*
 * Defines
 * */

#define PORT_E_BASE    0x40021000
#define GPIO_E_MODER    ((volatile unsigned int *)  (PORT_E_BASE))
#define GPIO_E_OTYPER   ((volatile unsigned short *) (PORT_E_BASE+0x4))
#define GPIO_E_OSPEEDR  ((volatile unsigned int *)  (PORT_E_BASE+0x8))
#define GPIO_E_PUPDR    ((volatile unsigned int *)  (PORT_E_BASE+0xC))  
#define GPIO_E_IDR_LOW  ((volatile unsigned short *) (PORT_E_BASE+0x10))
#define GPIO_E_IDR_HIGH ((volatile unsigned char *) (PORT_E_BASE+0x11))  
#define GPIO_E_ODR_LOW  ((volatile unsigned char *) (PORT_E_BASE+0x14))
#define GPIO_E_ODR_HIGH ((volatile unsigned char *) (PORT_E_BASE+0x15))
#define PORT_D_BASE     0x40020C00
#define GPIO_D_MODER    ((volatile unsigned int *)  (PORT_D_BASE))
#define GPIO_D_OTYPER   ((volatile unsigned short *) (PORT_D_BASE+0x4))
#define GPIO_D_PUPDR    ((volatile unsigned int *)  (PORT_D_BASE+0xC))
#define GPIO_D_HIGH_IDR ((volatile unsigned char *) (PORT_D_BASE+0x11))
#define GPIO_D_LOW_ODR  ((volatile unsigned char *) (PORT_D_BASE+0x14))
#define GPIO_D_HIGH_ODR ((volatile unsigned char *) (PORT_D_BASE+0x15))
#define SysTick         0xE000E010
#define STK_CTRL        ((volatile unsigned int *) (SysTick))
#define STK_LOAD        ((volatile unsigned int *) (SysTick+0x4))
#define STK_VAL         ((volatile unsigned int *) (SysTick+0x8))
#define STK_CALIB       ((volatile unsigned int *) (SysTick+0xC))

//Misc defines

#define SIMULATOR
#define MAX_POINTS 40

//Styrregister

#define B_E         0x40 // Enable
#define B_RST        0x20 // Reset
#define B_CS2        0x10 // Controller Select 2
#define B_CS1        8 // Controller Select 1
#define B_SELECT     4 // 0 Graphics, 1 ASCII
#define B_RW         2 // 0 Write, 1 Read
#define B_RS         1 // 0 Command, 1 Data

//LCD

#define LCD_ON          0x3F // Display on
#define LCD_OFF         0x3E // Display off
#define LCD_SET_ADD     0x40 // Set horizontal coordinate
#define LCD_SET_PAGE    0xB8 // Set vertical coordinate
#define LCD_DISP_START  0xC0 // Start address
#define LCD_BUSY        0x80 // Read busy status

/*
 * Typedefs
 */
typedef unsigned char uint8_t;

/*
 *     startup.c
 */
void startup(void) __attribute__((naked)) __attribute__((section (".start_section")) );

void startup ( void ) {
__asm volatile(
    " LDR R0,=0x2001C000\n"        /* set stack */
    " MOV SP,R0\n"
    " BL main\n"                /* call main */
    "_exit: B .\n"                /* never return */
    ) ;
}

typedef struct tPoint {
    
    unsigned char x;
    unsigned char y;
    
} POINT;

typedef struct tGeometry {
    
    int numpoints;
    int sizex;
    int sizey;
    POINT px[MAX_POINTS];
    
} GEOMETRY, *PGEOMETRY;

GEOMETRY ball_geometry = {
    
    12,
    4,4,
    {
        {0,1},{0,2},{1,0},{1,1},{1,2},{1,3},{2,0},{2,1},{2,2},{2,3},{3,1},{3,2}
    }
    
};

GEOMETRY pong_geometry = {
    
    40,
    10,20,
    {
        {0,1},{0,2},{0,3},{0,4},{0,5},{0,6},{0,7},{0,8},{0,9},{0,10},{1,0},{1,1},{1,2},{1,3},{1,4},{1,5},{1,6},{1,7},{1,8},{1,9},{1,10},
        {2,0},{2,1},{2,2},{2,3},{2,4},{2,5},{2,6},{2,7},{2,8},{2,9},{2,10}, //{3,1},{3,2},{3,3},{3,4},{3,5},{3,6},{3,7},{3,8},{3,9},{3,10},
    }

};

typedef struct tObj {
    
    PGEOMETRY geo;
    int dirx, diry;
    int posx, posy;
    void (* draw) (struct tObj *);
    void (* clear) (struct tObj *);
    void (* move) (struct tObj *);
    void (* set_speed) (struct tObj *, int, int);
    
} OBJECT, *POBJECT;

static void graphic_ctrl_bit_set(uint8_t x){
    unsigned char c;
    c = *GPIO_E_ODR_LOW;
    c &= ~B_SELECT;
    c |= (~B_SELECT & x);
    *GPIO_E_ODR_LOW = c;
}

static void graphic_ctrl_bit_clear(uint8_t x) {
    unsigned char c;
    c = *GPIO_E_ODR_LOW;
    c &= ~B_SELECT;
    c &= ~x;
    *GPIO_E_ODR_LOW = c;
}

void delay_250ns(void) {

    *STK_CTRL = 0;
    *STK_LOAD = ((168/4) -1);
    *STK_VAL = 0;
    *STK_CTRL = 5;
    while((*STK_CTRL & 0x10000) == 0);
    *STK_CTRL = 0;

}

void delay_500ns(void) {

    delay_250ns();
    delay_250ns();

}

void delay_mikro(unsigned int us) {

    #ifdef    SIMULATOR
    us = us / 1000;
    us++;
    #endif
    us = us / 1000;
    us++;
    while(us > 0){

        delay_250ns();
        delay_250ns();
        delay_250ns();
        delay_250ns();
        us--;

    }
}

void delay_milli(unsigned int ms) {

    #ifdef    SIMULATOR
    ms = ms / 250;
    ms++;
    #endif
    
    ms = ms / 2500;
    ms++;

    while(ms > 0){

        delay_mikro(1000);
        ms--;

    }
}

static void select_controller(uint8_t controller) {
    switch(controller) {
        case 0:
            graphic_ctrl_bit_clear(B_CS1|B_CS2);
            break;
        case B_CS1:
            graphic_ctrl_bit_set(B_CS1);
            graphic_ctrl_bit_clear(B_CS2);
            break;
        case B_CS2:
            graphic_ctrl_bit_set(B_CS2);
            graphic_ctrl_bit_clear(B_CS1);
            break;
        case B_CS1|B_CS2:
            graphic_ctrl_bit_set(B_CS1|B_CS2);
            break;
    }
}

static void graphic_wait_ready (void) {
    uint8_t c;
    graphic_ctrl_bit_clear(B_E);
    *GPIO_E_MODER = 0x00005555;
    graphic_ctrl_bit_clear(B_RS);
    graphic_ctrl_bit_set(B_RW);
    delay_500ns();
    
    while(1) {
        graphic_ctrl_bit_set(B_E);
        delay_500ns();
        c = *GPIO_E_IDR_HIGH & LCD_BUSY;
        graphic_ctrl_bit_clear(B_E);
        delay_500ns();
        if (c == 0) break;
    }
    graphic_ctrl_bit_set(B_E);
    *GPIO_E_MODER = 0x55555555;
}

static uint8_t graphic_read(uint8_t controller) {
    uint8_t c;
    graphic_ctrl_bit_clear(B_E);
    *GPIO_E_MODER = 0x00005555;
    graphic_ctrl_bit_set(B_RS|B_RW);
    select_controller(controller);
    delay_500ns();
    graphic_ctrl_bit_set(B_E);
    delay_500ns();
    c = *GPIO_E_IDR_HIGH;
    delay_500ns();
    graphic_ctrl_bit_clear(B_E);
    *GPIO_E_MODER = 0x55555555;
    
    if (controller & B_CS1) {
        select_controller(B_CS1);
        graphic_wait_ready();
    }
    if (controller & B_CS2) {
        select_controller(B_CS2);
        graphic_wait_ready();
    }
    return c;
}

static void graphic_write(uint8_t value, uint8_t controller) {
    *GPIO_E_ODR_HIGH = value;
    select_controller(controller);
    delay_500ns();
    graphic_ctrl_bit_set(B_E);
    delay_500ns();
    graphic_ctrl_bit_clear(B_E);
    
    if (controller | B_CS1) {
        select_controller(B_CS1);
        graphic_wait_ready();
    }
    if (controller | B_CS2) {
        select_controller(B_CS2);
        graphic_wait_ready();
    }
    *GPIO_E_ODR_HIGH = 0;
    graphic_ctrl_bit_set(B_E);
    select_controller(0);
}

static void graphic_write_command(uint8_t command, uint8_t controller) {
    graphic_ctrl_bit_clear(B_E);
    select_controller(controller);
    graphic_ctrl_bit_clear(B_RS|B_RW);
    graphic_write(command, controller);
}

static void graphic_write_data(uint8_t data, uint8_t controller) {
    graphic_ctrl_bit_clear(B_E);
    select_controller(controller);
    graphic_ctrl_bit_set(B_RS);
    graphic_ctrl_bit_clear(B_RW);
    graphic_write(data, controller);
}

static uint8_t graphic_read_data(uint8_t controller) {
    graphic_read(controller);
    return graphic_read(controller);
}

void ascii_ctrl_bit_set(unsigned char x) {
    unsigned char c;
    c = *GPIO_E_ODR_LOW;
    c |= (B_SELECT | x);
    *GPIO_E_ODR_LOW = c;
}

void ascii_ctrl_bit_clear(unsigned char x) {
    unsigned char c;
    c = *GPIO_E_ODR_LOW;
    c &= (B_SELECT | ~x);
    *GPIO_E_ODR_LOW = c;
}


void ascii_write_data(unsigned char data) {
    ascii_ctrl_bit_set(B_RS);
    ascii_ctrl_bit_clear(B_RW);
    ascii_write_controller(data);
}

unsigned char ascii_read_controller() {
    unsigned char c;
    ascii_ctrl_bit_set(B_E);
    delay_250ns();
    delay_250ns();
    c = *GPIO_E_IDR_HIGH;
    ascii_ctrl_bit_clear(B_E);
    return c;
}

unsigned char ascii_read_status() {
    *GPIO_E_MODER &= 0x00005555;
    ascii_ctrl_bit_clear(B_RS);
    ascii_ctrl_bit_set(B_RW);
    unsigned char c;
    c = ascii_read_controller();
    *GPIO_E_MODER |= 0x55550000;
    return c;
}

void ascii_write_cmd(unsigned char command) {
    ascii_ctrl_bit_clear(B_RS);
    ascii_ctrl_bit_clear(B_RW);
    ascii_write_controller(command);
}

unsigned char ascii_read_data() {
    *GPIO_E_MODER = 0x00005555;
    ascii_ctrl_bit_set(B_RS);
    ascii_ctrl_bit_set(B_RW);
    unsigned char c = ascii_read_controller();
    *GPIO_E_MODER = 0x55555555;
    return c;
}

void ascii_write_controller(unsigned char command) {
    ascii_ctrl_bit_set(B_E);
    *GPIO_E_ODR_HIGH = command;
    ascii_ctrl_bit_clear(B_E);
    delay_250ns();
}

void ascii_gotoxy(int x, int y) {
    unsigned char address;
    address = (x-1);
    if(y==2)
    {
        address = (0x40 + address);
    }
    ascii_write_cmd(0x80 | address);
}

void ascii_write_char(unsigned char c) {
    ascii_wait_ready();
    ascii_write_data(c);
    delay_mikro(39);
}

void ascii_wait_ready() {
    while((ascii_read_status() & 0x80)== 0x80);
    delay_mikro(8);
}

void asciiInit() {
    ascii_wait_ready();
    ascii_write_cmd( 0x38 ); /* Function set */
    delay_mikro( 39 );
    ascii_wait_ready();
    ascii_write_cmd( 0x0C ); /* Display on */
    delay_mikro( 39 );
    ascii_wait_ready();
    ascii_write_cmd( 1 ); /* Clear display */
    delay_milli( 2 );
    ascii_wait_ready();
    ascii_write_cmd( 6 ); /* Entry mode set */
    delay_mikro( 39 );
}

void app_init(void) {
    *GPIO_E_MODER = 0x55555555;
    *GPIO_E_OTYPER = 0x0;
    *GPIO_E_PUPDR = 0x00000000;
    *GPIO_D_MODER = 0x55005555;
    *GPIO_D_OTYPER = 0x0F;
    *GPIO_D_PUPDR = 0x55AA;
}

void graphic_initalize(void) {
    graphic_ctrl_bit_set(B_E);
    delay_mikro(10);
    graphic_ctrl_bit_clear(B_CS1|B_CS2|B_RST|B_E);
    delay_milli(30);
    graphic_ctrl_bit_set(B_RST);
    delay_milli(100);
    
    graphic_write_command(LCD_OFF, B_CS1|B_CS2);
    graphic_write_command(LCD_ON, B_CS1|B_CS2);
    graphic_write_command(LCD_DISP_START, B_CS1|B_CS2);
    graphic_write_command(LCD_SET_ADD, B_CS1|B_CS2);
    graphic_write_command(LCD_SET_PAGE, B_CS1|B_CS2);
    select_controller(0);
}

void graphic_clear_screen() {
    for (char page = 0; page<=7; page++) {
        graphic_write_command(LCD_SET_PAGE    |page    ,B_CS1|B_CS2);
        graphic_write_command(LCD_SET_ADD    |0        ,B_CS1|B_CS2);
        
        for (char add = 0; add<=63; add++) {
            graphic_write_data(0    , B_CS1|B_CS2);
        }
    }
}

void pixel(uint8_t x, uint8_t y, uint8_t set) {
    
    uint8_t mask, temp, controller;
    int index;
    
    if(x < 1 || x > 128 || y < 1 || y > 64) {
        return;
    }
    
    index = (y-1)/8;
    mask = 1 << ((y - 1) & 7);
    switch((y-1) % 8) {
        
        case 0:
            mask = 1;
            break;
        case 1:
            mask = 2;
            break;
        case 2:
            mask = 4;
            break;
        case 3:
            mask = 8;
            break;
        case 4:
            mask = 0x10;
            break;
        case 5:
            mask = 0x20;
            break;
        case 6:
            mask = 0x40;
            break;
        case 7:
            mask = 0x80;
            break;
        
    }
    
    if(!set) {
        mask = ~mask;
    }
    
    if(x > 64) {
        controller = B_CS2;
        x = x-65;
    }
    else {
        controller = B_CS1;
        x = x-1;
    }
    
    graphic_write_command(LCD_SET_ADD | x, controller);
    graphic_write_command(LCD_SET_PAGE  | index, controller);
    temp = graphic_read_data(controller);
    graphic_write_command(LCD_SET_ADD | x, controller);
    
    if(set) {
        mask |= temp;
    }
    else {
        mask &= temp;
    }
    
    graphic_write_data(mask, controller);
    
}

void set_object_speed(POBJECT o, int speedx, int speedy) {
    
    o->dirx = speedx;
    o->diry = speedy;
    
}

void draw_object(POBJECT o) {
    
    for(int i = 0; i < o->geo->numpoints; i++) {
        pixel(o->posx+o->geo->px[i].x, o->posy+o->geo->px[i].y, 1);
    }
    
}

void clear_object(POBJECT o) {
    
    for(int i = 0; i < o->geo->numpoints; i++) {
        pixel(o->posx+o->geo->px[i].x, o->posy+o->geo->px[i].y, 0);
    }
    
}

int player1_score = 0;
int player2_score = 0;
int player_score(char player, char inc, char o) {
    
        char p1win[] = "Player 1 wins!";
    char p2win[] = "Player 2 wins!";
    
    ascii_gotoxy(1,1);
    char *s;
    if (player == 1) s=p1win;
    if (player == 2) s=p2win;
    while(*s)
    {
        ascii_write_char(*s++);
    }
    while (1) {
        
    }

/*
    if (player==1) {
        if (inc == 1) {
            player1_score = player1_score + 1;
        }
        return player1_score;
    }
    if (player==2) {
        if (inc == 1) {
            player2_score = player2_score + 1;
        }
        return player2_score;
    }
    */
}

void collision_object(POBJECT ball, POBJECT ponger) {
    if (ball->posx == ponger->posx || ball->posx == ponger->posx + 1  || ball->posx == ponger->posx + 2 ||ball->posx == ponger->posx + 3 || ball->posx == ponger->posx - 1 ||ball->posx == ponger->posx -2 || ball->posx == ponger->posx -3) {
        if (ball->posy < ponger->posy || ball->posy > ponger->posy+10) {
            ball->dirx = -ball->dirx;
        }
    }
}

void move_object(POBJECT o) {
    
    clear_object(o);
    
    o->posx += o->dirx;
    o->posy += o->diry;
    
    if(o->posx < 0){
        o->dirx = -o->dirx;
        player_score(2, 1, 1);
        o->posx = 75;
        o->posy = 10;
    }
    if(o->posx > 128){
        o->dirx = -o->dirx;
        player_score(1, 1, 1);
        o->posx = 75;
        o->posy = 10;
    }
    if(o->posy < 0){
        o->diry = -o->diry;
    }
    if(o->posy > 64){
        o->diry = -o->diry;
    }
    draw_object(o);
}

static OBJECT ball = {
    
    &ball_geometry,
    0,0,
    10,10,
    draw_object,
    clear_object,
    move_object,
    set_object_speed
    
};

static OBJECT ponger = {
    
    &pong_geometry,
    0,0,
    1,1,
    draw_object,
    clear_object,
    move_object,
    set_object_speed
    
};

static OBJECT ponger2 = {
    
    &pong_geometry,
    0,0,
    122,1,
    draw_object,
    clear_object,
    move_object,
    set_object_speed
    
};

char ActivateRow(char row){
    
    switch(row){
        case 1:
            *GPIO_D_HIGH_ODR = 0x10;
            break;
        case 2:
            *GPIO_D_HIGH_ODR = 0x20;
            break;
        case 3:
            *GPIO_D_HIGH_ODR = 0x40;
            break;
        case 4:
            *GPIO_D_HIGH_ODR = 0x80;
            break;
        case 0:
            *GPIO_D_HIGH_ODR = 0x00;
            break;
        }
        
}

char ReadColumn(void){
    
    unsigned char c;
    c = *GPIO_D_HIGH_IDR;
    if(c & 0x8) return 4;
    if(c & 0x4) return 3;
    if(c & 0x2) return 2;
    if(c & 0x1) return 1;
    return 0;
    
}

unsigned char keyb (void){
    
    unsigned char key[] = {1,2,3,0xA,4,5,6,0xB,7,8,9,0xC,0xE,0,0xF,0xD};
    unsigned char col, row;
    
    for(row = 1; row<=4; row++){
        
        ActivateRow(row);
        if(col = ReadColumn()){
            
            ActivateRow(0);
            return key[4*(row-1)+(col-1)];
            
        }
        
    }
    
    ActivateRow(0);
    return 0xFF;
    
}

int main(int argc, char **argv){
    
    char c;
    char *s;

    POBJECT p = &ball;
    POBJECT player1 = &ponger;
    POBJECT player2 = &ponger2;
    app_init();
    graphic_initalize();
    asciiInit();

#ifndef SIMULATOR
    graphic_clear_screen();
#endif
    
    while(1){
        player1->move(player1); // creates and moves player 1
        p->move(p); // creates and moves ball
        player2->move(player2); // creates and moves player 2
        
        collision_object(p, player1); // see if the ball bounces at player1
        collision_object(p, player2); // see if the ball bounces at player2

        delay_milli(40);
        p->set_speed(p, 400, 1); // sets speed of ball
        c = keyb();
        switch(c) {
            
            case 1:
                player1->set_speed(player1, 0, -2);
                delay_milli(10);
                player1->set_speed(player1, 0, 0);
                break;
            case 4:
                player1->set_speed(player1, 0, 2);
                delay_milli(10);
                player1->set_speed(player1, 0, 0);
                break;
            case 3:
                player2->set_speed(player2, 0, -2);
                delay_milli(10);
                player2->set_speed(player2, 0, 0);
                break;
            case 6:
                player2->set_speed(player2, 0, 2);
                delay_milli(10);
                player2->set_speed(player2, 0, 0);
                break;

        }
        
    }
        
}
