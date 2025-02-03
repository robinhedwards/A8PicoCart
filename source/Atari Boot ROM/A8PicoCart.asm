/* Boot ROM for A8PicoCart
 * by Robin Edwards/Electrotrains@AtariAge
 * This file builds with WUDSN/MADS into an 8K Atari ROM
 * The 8k ROM should be converted into a C include file using:
 *  xxd -i A8PicoCart.rom > rom.h
 */

/*
 Theory of Operation
 -------------------
 Atari sends command to mcu on cart by writing to $D5DF ($D5E0-$D5FF = SDX)
 (extra paramters for the command in $D500-$D5DE)
 Atari must be running from RAM when it sends a command, since the mcu on the cart will
 go away at that point.
 Atari polls $D500 until it reads $11. At this point it knows the mcu is back
 and it is safe to rts back to code in cartridge ROM again.
 Results of the command are in $D501-$D5DF
*/

CART_CMD_OPEN_ITEM = $0
CART_CMD_READ_CUR_DIR = $1
CART_CMD_GET_DIR_ENTRY = $2
CART_CMD_UP_DIR = $3
CART_CMD_ROOT_DIR = $4
CART_CMD_SEARCH = $5
CART_CMD_LOAD_SOFT_OS = $10
CART_CMD_SOFT_OS_CHUNK = $11
CART_CMD_RESET_FLASH = $F0
CART_CMD_NO_CART = $FE
CART_CMD_ACTIVATE_CART = $FF

DIR_START_ROW = 7
DIR_END_ROW = 21
ITEMS_PER_PAGE = DIR_END_ROW-DIR_START_ROW+1

;@com.wudsn.ide.lng.outputfileextension=.rom

;CARTCS	= $bffa                    ;Start address vector, used if CARTFG has CARTFG_START_CART bit set
;CART	= $bffc                    ;Flag, must be zero for modules
;CARTFG	= $bffd                    ;Flags or-ed together, indicating how to start the module.
;CARTAD	= $bffe                    ;Initialization address vector

CARTFG_DIAGNOSTIC_CART = $80       ;Flag value: Directly jump via CARTAD during RESET.
CARTFG_START_CART      = $04       ;Flag value: Jump via CARTAD and then via CARTCS.
CARTFG_BOOT            = $01       ;Flag value: Boot peripherals, then start the module.

COLDSV = $E477				; Coldstart (powerup) entry point
WARMSV = $E474				; Warmstart entry point
CH = $2FC				; Internal hardware value for the last key pressed
BOOT = $09
CASINI = $02
OSROM = $C000
PORTB = $D301
NMIEN = $D40E
PMBASE = $D407
SDMCTL = $22F
GPRIOR = $26F
PCOLR0 = $2C0
PCOLR1 = $2C1
PCOLR2 = $2C2
PCOLR3 = $2C3
COLOR0 = $2C4
COLOR1 = $2C5
COLOR2 = $2C6
COLOR3 = $2C7
COLOR4 = $2C8
STICK0 = $278

HPosP0	equ $D000
HPosP1	equ $D001
HPosP2	equ $D002
HPosP3	equ $D003
HPosM0	equ $D004
HPosM1	equ $D005
HPosM2	equ $D006
HPosM3	equ $D007
SizeP0	equ $D008
SizeP1	equ $D009
SizeP2	equ $D00A
SizeP3	equ $D00B
SizeM	equ $D00C
GrafP0	equ $D00D
GrafP1	equ $D00E
GrafP2	equ $D00F
GrafP3	equ $D010
Trig0	equ $D010
ColPM2	equ $D014
GRACTL	equ $D01D

sm_ptr = $58				; screen memory
search_string = $600
wait_for_cart = $620			; routine copied here
reboot_to_selected_cart = $630		; routine copied here

PMBuffer = $800
Player0Data = $A00
Player1Data = $A80
Player2Data = $B00
Player3Data = $B80

; ************************ VARIABLES ****************************
num_dir_entries = $80
dir_entry	= $81
ypos		= $82
cur_ypos	= $83
top_item	= $84
cur_item	= $85
search_text_len	= $86
search_results_mode = $87
trigger_state	= $88
stick_state	= $89
stick_timer	= $8a
trigger_pressed	= $8b
stick_input	= $8c
tmp_ptr		= $90	// word
text_out_x	= $92	// word
text_out_y	= $94	// word
text_out_ptr	= $96	// word
text_out_len	= $98
cur_chunk	= $99

; XEX loader stuff from Jon Halliday/FJC
LoaderAddress	equ $700
VER_MAJ		equ $01
VER_MIN		equ $02
FMSZPG		equ $43
Critic		equ $42
IOPtr		equ FMSZPG
FileSize	equ FMSZPG+2 ; .ds 4
ptr1		equ FMSZPG
ptr2		equ FMSZPG+2
ptr3		equ FMSZPG+4
DOSINI		equ $0C
MEMLO		equ $02E7
RunVec		equ $02E0
IniVec		equ $02E2
VCOUNT		equ $D40B
WSYNC		equ $D40A
GINTLK		equ $03FA
TRIG3		equ $D013
SDLSTL		equ $230
CIOV		equ $E456

;	CIO Error Codes
	.enum IOErr
AlreadyOpen	= 129
NotOpen		= 133
EOF		= 136
NAK		= 139
NoFunction	= 146
BadName		= 165
NotFound	= 170
	.ende
	
	.struct IOCBlock
ID		.byte
DevNum		.byte
Command		.byte
Status		.byte
Address		.word
Put		.word	; put byte address
Len		.word
Aux1		.byte 	
Aux2		.byte
Aux3		.byte
Aux4		.byte
Aux5		.byte
Aux6		.byte
	.ends
	
	org $0340

IOCB	dta IOCBlock [8]

;	CIO commands

	.enum IOCommand
Open	= $03
GetText	= $05
Read	= $07
PutText	= $09
Write	= $0B
Close	= $0C
Status	= $0D
	.ende
	
; ************************ CODE ****************************


        opt h-                     ;Disable Atari COM/XEX file headers

        org $a000                  ;RD5 cartridge base
        opt f+                     ;Activate fill mode

;Cartridge initalization
;Only the minimum of the OS initialization is complete, you don't want to code here normally.
init    .proc
        rts
        .endp ; proc init
	
;Cartridge start
;RAM, graphics 0 and IOCB no for the editor (E:) are ready
start   .proc
	lda ColPM2
	cmp #1
	beq pal_cols
ntsc_cols
	mva #$6F COLOR1
	mva #$62 COLOR2
	jmp patch_boot
pal_cols
	mva #$4F COLOR1
	mva #$42 COLOR2

patch_boot	
	mva #3 BOOT ; patch reset - from mapping the atari (revised) appendix 11
	mwa #reset_routine CASINI
	
        jsr display_boot_screen
	jsr copy_wait_for_cart
	jsr copy_reboot_to_selected_cart
	jsr setup_pmg
	jsr init_joystick
	
; check for trigger pressed on startup to reset flash on cartridge
	lda Trig0
	bne read_current_directory
	jsr reset_flash_prompt
	
; read directory
read_current_directory
	mva #0 search_results_mode
	lda #CART_CMD_READ_CUR_DIR
	jsr wait_for_cart
check_read_dir
	lda $D501
	cmp #1	; check for error flag
	bne read_dir_ok
	jsr display_error_msg_from_cart
	jmp read_current_directory
	
read_dir_ok
	lda $D502
	sta num_dir_entries
	mva #0 top_item
	mva #0 cur_item
	
; display_directory
display_directory
	jsr output_header_text
	jsr clear_screen
	
	lda num_dir_entries
	bne dir_ok
	
no_dir	jsr output_empty_dir_msg
	jsr hide_pmg_cursor
	jmp main_loop
	
dir_ok	jsr output_directory
	jsr draw_cursor
	
main_loop
	jsr wait_for_vsync
	jsr GetKey
	beq check_joystick
	cmp #$1C ; cur up
	beq up_pressed
	cmp #'-'
	beq up_pressed
	
	cmp #$1D ; cur down
	beq down_pressed
	cmp #'='
	beq down_pressed
	
	cmp #'b'
	bne _1
	jmp back_pressed
_1	cmp #$1E ; cur left
	bne _2 
	jmp back_pressed

_2	cmp #$9B ; ret
	bne _3
	jmp return_pressed

_3	cmp #'x'
	bne _4
	jmp disable_pressed

_4	cmp #$1B ; esc
	bne _5
	jmp search_pressed
_5
check_joystick
	jsr read_joystick
	lda trigger_pressed
	cmp #1
	beq return_pressed
	
	lda stick_input
	and #$01
	bne up_pressed
	
	lda stick_input
	and #$02
	bne down_pressed
	
	jmp main_loop

down_pressed
	lda cur_item
	clc
	adc #1
	cmp num_dir_entries
	bcs main_loop
; single row down
	inc cur_item
; do we need to page down?
	lda cur_item
	sec
	sbc top_item
	clc	
	cmp #ITEMS_PER_PAGE
	beq page_down
	jsr draw_cursor
	jmp main_loop
page_down
	lda top_item
	clc
	adc #ITEMS_PER_PAGE
	sta top_item
	jmp display_directory

up_pressed
	lda cur_item
	cmp #0
	beq main_loop
; single row up
	dec cur_item
; do we need to page up
	lda cur_item
	cmp top_item
	bmi page_up
	jsr draw_cursor
	jmp main_loop
page_up
	lda top_item
	sec
	sbc #ITEMS_PER_PAGE
	sta top_item
	jmp display_directory
	
return_pressed
	lda num_dir_entries
	bne return_pressed_ok	; check for empty dir
	jmp main_loop
return_pressed_ok
	lda cur_item
	sta $D500
	lda #CART_CMD_OPEN_ITEM
	jsr wait_for_cart

	lda $D501 ; look at return code from cart
	cmp #0
	beq directory_changed
	cmp #1
	beq file_loaded
	cmp #2
	beq xex_loaded
	cmp #3
	beq atr_loaded
	; if we get here, there was an error
	jsr display_error_msg_from_cart
	jmp read_current_directory
directory_changed
	jmp read_current_directory
file_loaded
	jmp reboot_to_selected_cart
xex_loaded
	jmp launch_xex
atr_loaded
	jmp launch_atr
		
back_pressed
	lda search_results_mode
	cmp #1
	beq exit_search_results
	lda #CART_CMD_UP_DIR
	jsr wait_for_cart
exit_search_results
	jmp read_current_directory
	
disable_pressed
	lda #CART_CMD_NO_CART
	jsr wait_for_cart
	jmp reboot_to_selected_cart

launch_xex
	jsr disable_pmg
	jsr copy_XEX_loader
	jmp LoadBinaryFile
	
launch_atr
	jsr disable_pmg
	jsr copy_soft_rom
	cmp #0
	beq reboot_atr
	jmp read_current_directory
reboot_atr
	jmp reboot_to_selected_cart
		
search_pressed
	jsr output_search_box
	jsr get_search_string
	lda search_text_len
	cmp #0
	bne search
	jmp display_directory
search
	; copy search_text_len bytes from search_text to $D5xx
	ldy #0
@	lda search_string,y
	sta $D500,y
	iny
	tya
	cmp search_text_len
	bcc @-
	; null terminate
	lda #0
	sta $D500,y
		
	jsr clear_screen
	jsr output_searching_msg
		
	lda #CART_CMD_SEARCH
	jsr wait_for_cart
	mva #1 search_results_mode
	jmp check_read_dir
        .endp ; proc start


; ************************ SUBROUTINES ****************************
.proc init_joystick
	mva #$01 trigger_state
	mva #$0F stick_state
	mva #0 stick_timer
	rts
	.endp

.proc read_joystick
	mva #0 trigger_pressed
	mva #0 stick_input
	
	lda stick_timer
	beq @+
	dec stick_timer

@	
	lda Trig0
	cmp trigger_state
	bne trigger_change	; trigger change

	lda STICK0
	and #$0F
	cmp stick_state
	bne stick_change	; stick change
	ldy stick_timer
	beq stick_change
	rts
stick_change
	sta stick_state
	EOR #$0F
	sta stick_input
	ldy #8
	sty stick_timer
	rts
trigger_change			; trigger has either gone up or down
	sta trigger_state
	cmp #0
	bne done
	mva #1 trigger_pressed
done
	rts
	.endp

.proc wait_for_vsync
	lda VCount
	rne
	lda VCount
	req
	rts
	.endp

.proc	copy_soft_rom
	lda #CART_CMD_LOAD_SOFT_OS
	jsr wait_for_cart
	lda $D501
	cmp #1	; check for error flag
	bne read_ok
	jsr display_error_msg_from_cart
	lda #1
	rts
read_ok
	; the following is from Appendix 12 of Mapping the Atari (revised), pg218
swap	php	; save processor status
	sei	; disable irqs
	lda NMIEN
	pha	; save NMIEN
	lda #0
	sta NMIEN
	; set colors
	mva #$B2 $D017
	mva #$B2 $D018
	; switch ROM to RAM
	LDA PORTB
	AND #$FE
	STA PORTB
	; copy

	mwa #OSROM tmp_ptr
	mva #0 cur_chunk
copy_page
	lda tmp_ptr+1
	cmp #$D0
	bne copy_first_half
	; skip $D000-D800
	lda #$D8
	sta tmp_ptr+1
	lda #$30
	sta cur_chunk
copy_first_half
	lda #$00
	sta tmp_ptr
	; fetch the first 128 bytes from the cartridge
	lda cur_chunk
	sta $D500
	lda #CART_CMD_SOFT_OS_CHUNK
	jsr wait_for_cart
	ldy #0
@	lda $D501,y
	sta (tmp_ptr),y
	iny
	cpy #128
	bne @-
	inc cur_chunk
copy_second_half
	lda #$80
	sta tmp_ptr
	; fetch the next 128 bytes from the cartridge
	lda cur_chunk
	sta $D500
	lda #CART_CMD_SOFT_OS_CHUNK
	jsr wait_for_cart
	ldy #0
@	lda $D501,y
	sta (tmp_ptr),y
	iny
	cpy #128
	bne @-
	inc cur_chunk
	; move to the next page
	inc tmp_ptr+1
	bne copy_page
	
enable	pla
	sta NMIEN
	plp
	lda #0
	rts
	.endp
	
.proc   get_search_string
	mva #0 search_text_len
	jmp output
loop
	jsr GetKey
	beq loop
	cmp #$1B ; esc
	beq cancel
	cmp #$7E; del
	beq delete
	cmp #$9B ; ret
	beq done
	ldy search_text_len
	cpy #14
	beq loop
	jmp newchar
delete	
	lda search_text_len
	beq loop
	; erase character
	clc
	adc #16
	sta text_out_x
	mwa #cursor_text text_out_ptr
	mva #1 text_out_len
	jsr output_text
	
	dec search_text_len
	jmp output
newchar		
	ldy search_text_len
	sta search_string,y
	inc search_text_len
output	
	mva #16 text_out_x
	mva #9 text_out_y
	mwa #search_string text_out_ptr
	mva search_text_len text_out_len
	jsr output_text
	; draw cursor
	lda text_out_x
	clc
	adc text_out_len
	sta text_out_x
	mwa #cursor_text text_out_ptr
	mva #1 text_out_len
	jsr output_text_inverted
	
	jmp loop
cancel	mva #0 search_text_len
done	rts
	.endp
.proc	reset_routine
	mva #3 BOOT
	lda #CART_CMD_ROOT_DIR ; tell the mcu we've done a reset
	jsr wait_for_cart
	rts
	.endp

.proc	display_error_msg_from_cart
	jsr hide_pmg_cursor
	mva #1 text_out_x
	mva #8 text_out_y
	mwa #error_text1 text_out_ptr
	mva #(.len error_text1) text_out_len
	jsr output_text_internal
	inc text_out_y
	mwa #error_text2 text_out_ptr
	mva #(.len error_text2) text_out_len
	jsr output_text_internal
	inc text_out_y
	mwa #error_text3 text_out_ptr
	mva #(.len error_text3) text_out_len
	jsr output_text_internal
	; display the actual errro
	mva #8 text_out_x
	mva #9 text_out_y
	mwa #$D502 text_out_ptr
	mva #30 text_out_len
	jsr output_text
	jsr wait_key
	rts
	.endp
	
.proc	reset_flash_prompt
	jsr hide_pmg_cursor
	mva #1 text_out_x
	mva #8 text_out_y
	mwa #reset_flash_text1 text_out_ptr
	mva #(.len reset_flash_text1) text_out_len
	jsr output_text_internal
	inc text_out_y
	mwa #reset_flash_text2 text_out_ptr
	mva #(.len reset_flash_text2) text_out_len
	jsr output_text_internal
	inc text_out_y
	mwa #reset_flash_text3 text_out_ptr
	mva #(.len reset_flash_text3) text_out_len
	jsr output_text_internal
@	jsr GetKey
	beq @-
	cmp #'r'
	beq reset
	rts

reset	lda #CART_CMD_RESET_FLASH
	jsr wait_for_cart
	rts
	.endp
	
.proc	output_directory
	mva top_item dir_entry
	mva #DIR_START_ROW ypos
next_entry
	ldy ypos
	dey
	tya
	cmp #DIR_END_ROW
	beq end_of_page
	lda dir_entry
	cmp num_dir_entries
	beq end_of_page
	sta $D500
	lda #CART_CMD_GET_DIR_ENTRY ; request from mcu
	jsr wait_for_cart
	
; output the directory entry
	mva ypos text_out_y
	mva #4 text_out_x
	ldx $D501 ; 0 = file, 1 = folder
	mwa #$D502 text_out_ptr
;	mwa #test_text text_out_ptr
	mva #31 text_out_len
	cpx #1
	beq folder
file	jsr output_text
	jmp next
folder	jsr output_text
	mva #0 text_out_x
	mva #3 text_out_len
	mwa #folder_text text_out_ptr
	jsr output_text_inverted
next	inc ypos
	inc dir_entry
	jmp next_entry
end_of_page
	rts
	.endp

; clear screen
clear_screen .proc
	mwa sm_ptr tmp_ptr
	ldy #DIR_START_ROW
@	dey
	bmi yend
	adw tmp_ptr #40
	jmp @-
yend
	ldx #ITEMS_PER_PAGE	; number of lines to clear
yloop	lda #0
	ldy #39
xloop	sta (tmp_ptr),y
	dey
	bpl xloop
	adw tmp_ptr #40
	dex
	bne yloop
	rts
	.endp

.proc 	draw_cursor
	lda cur_item
	sec
	sbc top_item
	clc
	adc #DIR_START_ROW
	sta cur_ypos
	jsr draw_pmg_cursor
	rts
	.endp

;	Scan keyboard (returns N = 1 for no key pressed, else ASCII in A)
.proc	GetKey
	ldx CH
	cpx #$FF
	beq NoKey
	mva #$FF CH		; set last key pressed to none
	lda scancodes,x
	cmp #$FF
NoKey
	rts
	.endp
	
.proc	setup_pmg 
	mva #>PMBuffer PMBASE	
	mva #$2E SDMCTL
	
	mva #$3 SizeP0
	mva #$48 PCOLR0
	mva #$40 HPosP0
	
	mva #$3 SizeP1
	mva #$48 PCOLR1
	mva #$60 HPosP1
	
	mva #$3 SizeP2
	mva #$48 PCOLR2
	mva #$80 HPosP2
	
	mva #$3 SizeP3
	mva #$48 PCOLR3
	mva #$A0 HPosP3
	
	mva #$1 GPRIOR
	rts
	.endp

.proc	disable_pmg
	mva #34 SDMCTL
	lda #0
	sta GRACTL
	ldy #$0c
@
	sta $D000,y
	dey
	bpl @-
	
	mva #$0 PCOLR0
	mva #$0 PCOLR1
	mva #$0 PCOLR2
	mva #$0 PCOLR3
	
	lda:cmp:req 20	; wait vbl
	rts
	.endp
	
.proc	hide_pmg_cursor
	lda:cmp:req 20	; wait vbl
	mva #$0 GRACTL
	rts
	.endp
	
.proc	draw_pmg_cursor
	lda:cmp:req 20	; wait vbl
	mva #$3 GRACTL
	lda #0 ; clear pmg memory
	ldy #127
@	sta Player0Data,y
	sta Player1Data,y
	sta Player2Data,y
	sta Player3Data,y
	dey
	bpl @-

	lda #$C ; offset row 0
	ldy cur_ypos
	clc
@	adc #4	; skip character lines
	dey
	bpl @-
	tay
	
	lda #$FF ; draw
	ldx #3
@	sta Player0Data,y
	sta Player1Data,y
	sta Player2Data,y
	sta Player3Data,y
	iny
	dex
	bpl @-
	rts
	.endp

.proc	display_boot_screen
	mva #0 text_out_x
	mva #0 text_out_y
	mwa #menu_text1 text_out_ptr
	mva #(.len menu_text1) text_out_len
	jsr output_text_internal
	inc text_out_y
	mwa #menu_text2 text_out_ptr
	mva #(.len menu_text2) text_out_len
	jsr output_text_internal
	inc text_out_y
	mwa #menu_text3 text_out_ptr
	mva #(.len menu_text3) text_out_len
	jsr output_text_internal
	inc text_out_y
	mwa #menu_text4 text_out_ptr
	mva #(.len menu_text4) text_out_len
	jsr output_text_internal
	inc text_out_y
	mwa #menu_text5 text_out_ptr
	mva #(.len menu_text5) text_out_len
	jsr output_text_internal
	mva #23 text_out_y
	mwa #menu_text_bottom text_out_ptr
	mva #(.len menu_text_bottom) text_out_len
	jsr output_text_inverted
	rts
	.endp

.proc	output_header_text
	mva #9 text_out_x
	mva #DIR_START_ROW-2 text_out_y
	lda search_results_mode
	bne _2
_1	mwa #directory_text text_out_ptr
	mva #(.len directory_text) text_out_len
	jmp out
_2	mwa #search_results_text text_out_ptr
	mva #(.len search_results_text) text_out_len
out	jsr output_text_inverted
	rts
	.endp

.proc	output_empty_dir_msg
	mva #6 text_out_x
	mva #DIR_START_ROW+1 text_out_y
	mwa #empty_dir_text text_out_ptr
	mva #(.len empty_dir_text) text_out_len
	jsr output_text
	rts
	.endp

.proc	output_searching_msg
	mva #12 text_out_x
	mva #9 text_out_y
	mwa #searching_text text_out_ptr
	mva #(.len searching_text) text_out_len
	jsr output_text
	rts
	.endp

.proc	output_search_box
	jsr hide_pmg_cursor
	mva #8 text_out_x
	mva #8 text_out_y
	mwa #search_text1 text_out_ptr
	mva #(.len search_text1) text_out_len
	jsr output_text_internal
	inc text_out_y
	mwa #search_text2 text_out_ptr
	mva #(.len search_text2) text_out_len
	jsr output_text_internal
	inc text_out_y
	mwa #search_text3 text_out_ptr
	mva #(.len search_text3) text_out_len
	jsr output_text_internal
	rts
	.endp

	
.proc	wait_key
	mva #$FF CH		; set last key pressed to none
@	ldx CH
	cpx #$FF
	beq @-
	mva #$FF CH
	rts
	.endp

; output text in text_out_ptr at (cur_x, cur_y)
.proc	output_text_internal
	mwa sm_ptr tmp_ptr
; add the cursor y offset
	ldy text_out_y
yloop	dey
	bmi yend
	adw tmp_ptr #40
	jmp yloop
yend	adw text_out_x tmp_ptr tmp_ptr ; add the cursor x offset
; text output loop
	ldy #0
nextchar ; text output loop
	lda (text_out_ptr),y
	sta (tmp_ptr),y
	iny
	cpy text_out_len
	bne nextchar
endoftext	
	rts
	.endp

; output text in text_out_ptr at (cur_x, cur_y)
.proc	output_text
	lda text_out_len
	bne ok
	rts
ok	mwa sm_ptr tmp_ptr
; add the cursor y offset
	ldy text_out_y
yloop	dey
	bmi yend
	adw tmp_ptr #40
	jmp yloop
yend	adw text_out_x tmp_ptr tmp_ptr ; add the cursor x offset
; text output loop
	ldy #0
nextchar ; text output loop
	lda (text_out_ptr),y
	beq endoftext ; end of line?
	cmp #96; convert ascii->internal
	bcs lower
	sec
	sbc #32
lower	sta (tmp_ptr),y
	iny
	cpy text_out_len
	bne nextchar
endoftext	
	rts
	.endp

; output text in text_out_ptr at (cur_x, cur_y)
output_text_inverted .proc 
	mwa sm_ptr tmp_ptr
; add the cursor y offset
	ldy text_out_y
yloop	dey
	bmi yend
	adw tmp_ptr #40
	jmp yloop
yend	adw text_out_x tmp_ptr tmp_ptr ; add the cursor x offset
; text output loop
	ldy #0
nextchar ; text output loop
	lda (text_out_ptr),y
	beq endoftext ; end of line?
	cmp #96; convert ascii->internal
	bcs lower
	sec
	sbc #32
lower	ora #$80
	sta (tmp_ptr),y
	iny
	cpy text_out_len
	bne nextchar
endoftext	
	rts
	.endp

.proc	copy_wait_for_cart
	ldy #.len[WaitForCartCode]
@
	lda WaitForCartCode-1,y
	sta wait_for_cart-1,y
	dey
	bne @-
	rts
	.endp
	
; cmd is in Accumulator
.proc WaitForCartCode
	sta $D5DF	; send cmd to the cart
@	lda $D500
	cmp #$11	; wait for the cart to signal it's back
	bne @-
	rts
	.endp

.proc	copy_reboot_to_selected_cart
	ldy #.len[RebootToSelectedCartCode]
@
	lda RebootToSelectedCartCode-1,y
	sta reboot_to_selected_cart-1,y
	dey
	bne @-
	rts
	.endp
	
.proc RebootToSelectedCartCode
	sei				; prevent GINTLK check in deferred vbi
	lda #CART_CMD_ACTIVATE_CART	; tell the cart we're ready for it switch ROM
	sta $D5DF
	jmp COLDSV
	.endp

; ************************ XEX LOADER ****************************

.proc copy_XEX_loader
	mwa #LoaderCodeStart ptr1
	mwa #LoaderAddress ptr2
	mwa #[EndLoaderCode-LoaderCode] ptr3
	jmp UMove
	.endp
	
; Move bytes from ptr1 to ptr2, length ptr3
.proc UMove
	lda ptr3
	eor #$FF
	adc #1
	sta ptr3
	lda ptr3+1
	eor #$FF
	adc #0
	sta ptr3+1
	
	ldy #0
Loop
	lda (ptr1),y
	sta (ptr2),y
	iny
	bne @+
	inc ptr1+1
	inc ptr2+1
@
	inc ptr3
	bne Loop
	inc ptr3+1
	bne Loop
	rts
	.endp


; ************************ DATA ****************************
	.local menu_text1
	.byte "   _   ___ ___ _       ___          _   "
	.endl
	.local menu_text2
	.byte "  /_\ ( _ ) _ (_)__ _ / __|__ _ _ _| |_ "
	.endl
	.local menu_text3
	.byte " / _ \/ _ \  _/ / _/_\ (__/ _' | '_|  _|"
	.endl
	.local menu_text4
	.byte "/_/ \_\___/_| |_\__\_/\___\__,_|_|  \__|"
	.endl
	.local menu_text5
	.byte "                      Electrotrains 2023"
	.endl
	.local menu_text_bottom
	.byte 'CurUp/Dn/Retn=Sel B=Back X=Boot Esc=Find'
	.endl
	.local directory_text
	.byte '[Directory contents]'
	.endl
	.local search_results_text
	.byte '[  Search results  ]'
	.endl
	
	.local error_text1
	.byte 81,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,69
	.endl
	.local error_text2
	.byte 124,"Error:",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,124
	.endl
	.local error_text3
	.byte 90,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,"P"+$80,"r"+$80,"e"+$80,"s"+$80,"s"+$80," "+$80,"a"+$80," "+$80,"k"+$80,"e"+$80,"y"+$80,67
	.endl
	
	.local search_text1
	.byte 81,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,69
	.endl
	.local search_text2
	.byte 124,"Search:",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,124
	.endl
	.local search_text3
	.byte 90,82,82,82,82,82,82,82,82,82,82,82,82,"E"+$80,"S"+$80,"C"+$80," "+$80,"C"+$80,"a"+$80,"n"+$80,"c"+$80,"e"+$80,"l"+$80,67
	.endl

	.local reset_flash_text1
	.byte 81,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,69
	.endl
	.local reset_flash_text2
	.byte 124,"Reset flash memory?                 ",124
	.endl
	.local reset_flash_text3
	.byte 90,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,82,"P"+$80,"r"+$80,"e"+$80,"s"+$80,"s"+$80," "+$80,"R"+$80," "+$80,"t"+$80,"o"+$80," "+$80, "r"+$80, "e"+$80, "s"+$80, "e"+$80, "t"+$80,67
	.endl

	
	.local folder_text
	.byte 'DIR',0
	.endl
	
	.local empty_dir_text
	.byte 'No valid files to display'
	.endl
	
	.local searching_text
	.byte 'Searching....'
	.endl
	
	.local cursor_text
	.byte ' '
	.endl
	
	.local test_text
	.byte 'Hello',0
	.endl
scancodes
	ins 'keytable.bin'
	
	
LoaderCodeStart

	opt f-
	org LoaderAddress
	opt f+
	
LoaderCode
	.byte 'L'
	.byte VER_MAJ
	.byte VER_MIN

	.proc LoadBinaryFile
	jsr InitLoader
Loop
	mwa #Return IniVec	; reset init vector
	jsr ReadBlock
	bmi Error
	cpw RunVec #Return
	bne @+
	mwa BStart RunVec	; set run address to start of first block
@
	jsr DoInit
	jmp Loop
Error
	jmp (RunVec)
Return
	rts
	.endp
	
	
//
//	Jump through init vector
//
	
	.proc DoInit
	jmp (IniVec)
	.endp


//
//	Read block from executable
//

	.proc ReadBlock
	jsr ReadWord
	bmi Error
	lda HeaderBuf
	and HeaderBuf+1
	cmp #$ff
	bne NoSignature
	jsr ReadWord
	bmi Error
NoSignature
	mwa HeaderBuf BStart
	jsr ReadWord
	bmi Error
	sbw HeaderBuf BStart BLen
	inw BLen
	mwa BStart IOPtr
	jsr ReadBuffer
Error
	rts
	.endp
	
	
	
	
//
//	Read word from XEX
//

	.proc ReadWord
	mwa #HeaderBuf IOPtr
	mwa #2 BLen		; fall into ReadBuffer
	.endp



//
//	Read buffer from XEX
//	Returns Z=1 on EOF
//
	
	.proc ReadBuffer
	jsr SetSegment
Loop
	lda BLen
	ora BLen+1
	beq Done
	
	lda FileSize		; first ensure we're not at the end of the file
	ora FileSize+1
	ora FileSize+2
	ora FileSize+3
	beq EOF

	inc BufIndex
	bne NoBurst			; don't burst unless we're at the end of the buffer
	
BurstLoop
	inc SegmentLo			; bump segment if we reached end of buffer
	bne @+
	inc SegmentHi
@
	jsr SetSegment

	lda Blen+1		; see if we can burst read the next 256 bytes
	beq NoBurst
	lda FileSize+1	; ensure buffer and remaining bytes in file are both >= 256
	ora FileSize+2
	ora FileSize+3
	beq NoBurst

	ldy #0			; read a whole page into RAM
@
	lda $D500,y		; doesn't matter about speculative reads (?)
	sta (IOPtr),y
	iny
	bne @-
	inc IOPtr+1		; bump address for next time

	ldx #3			; y is already 0
	sec
@
	lda FileSize,y	; reduce file size by 256
	sbc L256,y
	sta FileSize,y
	iny
	dex
	bpl @-
	dec Blen+1		; reduce buffer length by 256
	dec BufIndex
	bne ReadBuffer

NoBurst
	lda $D500
BufIndex	equ *-2
	ldy #0
	sta (IOPtr),y
	inw IOPtr
.nowarn	dew BLen
	
	ldx #3		; y is already 0
	sec
@
	lda FileSize,y
	sbc L1,y
	sta FileSize,y
	iny
	dex
	bpl @-
	bmi Loop
	
Done
	ldy #1
	rts
EOF
	ldy #IOErr.EOF
	rts
	
SetSegment
	ldy #0
SegmentLo equ *-1
	ldx #0
SegmentHi equ *-1
	sty $D500
	stx $D501
	rts
L1
	.dword 1
L256
	.dword 256
	.endp


HeaderBuf	.word 0
BStart		.word 0
BLen		.word 0



; Everything beyond here can be obliterated safely during the load
	
//
//	Loader initialisation
//
	
	.proc InitLoader
	sei
	lda #CART_CMD_ACTIVATE_CART
	sta $D5DF

	jsr SetGintlk
	jsr BasicOff
	cli
	jsr OpenEditor
	mwa #EndLoaderCode MEMLO
	mwa #LoadBinaryFile.Return RunVec	; reset run vector
	ldy #0
	tya
@
	sta $80,y
	iny
	bpl @-
	jsr ClearRAM
	
	ldy #3
@
	lda $D500,y
	sta FileSize,y
	dey
	bpl @-
	mva #3 ReadBuffer.BufIndex
	rts
	.endp

	
	
	.proc BASICOff
	mva #$01 $3f8
	mva #$C0 $6A
	lda portb
	ora #$02
	sta portb
	rts
	.endp
	
	

	.proc SetGintlk
	sta WSYNC
	sta WSYNC
	lda TRIG3
	sta GINTLK
	rts
	.endp
	
	
	
	.proc ClearRAM
	mwa #EndLoaderCode ptr1
;	sbw $c000 ptr1 ptr2
	sbw SDLSTL ptr1 ptr2		; clear up to display list address
	
	lda ptr2
	eor #$FF
	clc
	adc #1
	sta ptr2
	lda ptr2+1
	eor #$FF
	adc #0
	sta ptr2+1
	ldy #0
	tya
Loop
	sta (ptr1),y
	iny
	bne @+
	inc ptr1+1
@
	inc ptr2
	bne Loop
	inc ptr2+1
	bne Loop
	rts
	.endp
	
	
	
	.proc OpenEditor
	ldx #0
	lda #$0c
	sta iocb[0].Command
	jsr ciov
	mwa #EName iocb[0].Address
	mva #$0C iocb[0].Aux1
	mva #$00 iocb[0].Aux2
	mva #$03 iocb[0].Command
	jmp ciov

EName
	.byte 'E:',$9B

	.endp
	


	.if 0
//
//	Wait for sync
//

	.proc WaitForSync2
	lda VCount
	rne
	lda VCount
	req
	rts
	.endp
	
	.endif
	

	

EndLoaderCode ; end of relocated code

LoaderCodeSize	= EndLoaderCode-LoaderCode
	
	opt f-
	org LoaderCodeStart + LoaderCodeSize
	opt f+
	

; ************************ CARTRIDGE CONTROL BLOCK *****************

        org $bffa                 ;Cartridge control block
        .word start               ;CARTCS
        .byte 0                   ;CART
        .byte CARTFG_START_CART   ;CARTFG
        .word init                ;CARTAD

