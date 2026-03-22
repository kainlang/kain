; Z80 Test Assembly - Arcade Game Example
; Demonstrates Z80 opcodes, macros, conditionals, and data tables

; Constants
SCREEN_WIDTH EQU 256
SCREEN_HEIGHT EQU 192
SPRITE_SIZE EQU 16

; Conditional compilation
DEBUG EQU 1

; Macro definitions
InitSprite: MACRO
    LD A, \1
    LD (sprite_x), A
    LD A, \2
    LD (sprite_y), A
ENDM

; Main program
ORG $0000

Start:
    DI                  ; Disable interrupts
    LD SP, $FFFF        ; Initialize stack pointer
    CALL InitVideo
    CALL InitSound
    EI                  ; Enable interrupts
    IM 1                ; Interrupt mode 1

MainLoop:
    CALL ReadInput
    CALL UpdateGame
    CALL RenderFrame
    JP MainLoop

; Video initialization
InitVideo:
    LD HL, $4000        ; Video RAM base
    LD BC, $1800        ; Clear 6KB
.clear_loop:
    LD (HL), 0
    INC HL
    DEC BC
    LD A, B
    OR C
    JR NZ, .clear_loop
    RET

; Sound initialization
InitSound:
    LD A, $00
    OUT ($10), A        ; Sound chip register
    LD A, $FF
    OUT ($11), A        ; Sound chip data
    RET

; Input handling
ReadInput:
    IN A, ($20)         ; Read joystick port
    BIT 0, A            ; Test UP button
    CALL NZ, MoveUp
    BIT 1, A            ; Test DOWN button
    CALL NZ, MoveDown
    BIT 4, A            ; Test FIRE button
    CALL NZ, FireWeapon
    RET

; Movement routines
MoveUp:
    LD A, (sprite_y)
    DEC A
    CP 0
    RET Z               ; Don't move past top
    LD (sprite_y), A
    RET

MoveDown:
    LD A, (sprite_y)
    INC A
    CP SCREEN_HEIGHT - SPRITE_SIZE
    RET Z               ; Don't move past bottom
    LD (sprite_y), A
    RET

; Weapon system
FireWeapon:
    LD A, (bullet_active)
    OR A
    RET NZ              ; Bullet already active
    LD A, 1
    LD (bullet_active), A
    LD A, (sprite_x)
    ADD A, SPRITE_SIZE
    LD (bullet_x), A
    LD A, (sprite_y)
    ADD A, SPRITE_SIZE / 2
    LD (bullet_y), A
    RET

; Game update logic
UpdateGame:
    CALL UpdateBullet
    CALL CheckCollisions
    RET

UpdateBullet:
    LD A, (bullet_active)
    OR A
    RET Z               ; No active bullet
    LD A, (bullet_x)
    ADD A, 4            ; Bullet speed
    LD (bullet_x), A
    CP SCREEN_WIDTH
    RET C               ; Still on screen
    XOR A
    LD (bullet_active), A
    RET

CheckCollisions:
    ; Simplified collision detection
    LD A, (enemy_x)
    LD B, A
    LD A, (bullet_x)
    SUB B
    CP SPRITE_SIZE
    RET NC              ; No collision
    LD A, (enemy_y)
    LD B, A
    LD A, (bullet_y)
    SUB B
    CP SPRITE_SIZE
    RET NC              ; No collision
    CALL EnemyDestroyed
    RET

EnemyDestroyed:
    LD HL, (score)
    LD BC, 100
    ADD HL, BC
    LD (score), HL
    CALL PlayExplosionSound
    RET

PlayExplosionSound:
    LD B, 10
.sound_loop:
    LD A, B
    OUT ($10), A
    DJNZ .sound_loop
    RET

; Rendering
RenderFrame:
    CALL RenderSprite
    CALL RenderBullet
    CALL RenderEnemy
    RET

RenderSprite:
    LD A, (sprite_x)
    LD B, A
    LD A, (sprite_y)
    LD C, A
    LD HL, sprite_data
    CALL DrawSprite
    RET

RenderBullet:
    LD A, (bullet_active)
    OR A
    RET Z
    LD A, (bullet_x)
    LD B, A
    LD A, (bullet_y)
    LD C, A
    LD HL, bullet_data
    CALL DrawSprite
    RET

RenderEnemy:
    LD A, (enemy_x)
    LD B, A
    LD A, (enemy_y)
    LD C, A
    LD HL, enemy_data
    CALL DrawSprite
    RET

; Generic sprite drawing routine
DrawSprite:
    PUSH BC
    PUSH HL
    LD A, SPRITE_SIZE
    LD D, A
.row_loop:
    LD E, SPRITE_SIZE
.col_loop:
    LD A, (HL)
    OR A
    JR Z, .skip_pixel
    ; Draw pixel at (B, C)
    PUSH BC
    CALL PlotPixel
    POP BC
.skip_pixel:
    INC HL
    INC B
    DEC E
    JR NZ, .col_loop
    POP HL
    PUSH HL
    LD A, B
    SUB SPRITE_SIZE
    LD B, A
    INC C
    DEC D
    JR NZ, .row_loop
    POP HL
    POP BC
    RET

PlotPixel:
    ; B = X, C = Y
    PUSH BC
    LD A, C
    AND $07
    LD E, A
    LD A, C
    RRA
    RRA
    RRA
    AND $1F
    LD D, A
    LD A, B
    RRA
    RRA
    RRA
    AND $1F
    OR D
    LD D, A
    LD A, $40
    OR D
    LD D, A
    LD A, B
    AND $07
    LD B, A
    LD A, $80
.shift_loop:
    RRA
    DJNZ .shift_loop
    LD B, A
    LD A, (DE)
    OR B
    LD (DE), A
    POP BC
    RET

; Conditional debug code
IF DEBUG
DebugPrint:
    LD A, (debug_value)
    OUT ($FF), A
    RET
ENDIF

; Data tables
sprite_data:
    DB $00, $00, $3C, $7E, $FF, $FF, $7E, $3C
    DB $00, $00, $3C, $7E, $FF, $FF, $7E, $3C

bullet_data:
    DB $18, $3C, $3C, $18

enemy_data:
    DB $00, $7E, $FF, $FF, $FF, $FF, $7E, $00
    DB $00, $7E, $FF, $FF, $FF, $FF, $7E, $00

; Variables
sprite_x:       DB 10
sprite_y:       DB 100
bullet_x:       DB 0
bullet_y:       DB 0
bullet_active:  DB 0
enemy_x:        DB 200
enemy_y:        DB 100
score:          DW 0
debug_value:    DB 0

; Interrupt handler
ORG $0038
InterruptHandler:
    PUSH AF
    PUSH BC
    PUSH DE
    PUSH HL
    ; Handle interrupt
    CALL UpdateTimer
    POP HL
    POP DE
    POP BC
    POP AF
    RETI

UpdateTimer:
    LD HL, (timer_ticks)
    INC HL
    LD (timer_ticks), HL
    RET

timer_ticks:    DW 0
