/* $Id: mem.c,v 1.2 2009-08-08 06:49:44 masamic Exp $ */

/*
 * $Log: not supported by cvs2svn $
 * Revision 1.1.1.1  2001/05/23 11:22:08  masamic
 * First imported source code and docs
 *
 * Revision 1.4  1999/12/07  12:47:22  yfujii
 * *** empty log message ***
 *
 * Revision 1.4  1999/11/29  06:18:06  yfujii
 * Calling CloseHandle instead of fclose when abort().
 *
 * Revision 1.3  1999/11/01  06:23:33  yfujii
 * Some debugging functions are introduced.
 *
 * Revision 1.2  1999/10/18  03:24:40  yfujii
 * Added RCS keywords and modified for WIN32 a little.
 *
 */

#undef	MAIN

#include <stdio.h>
#include "run68.h"

static	int	mem_red_chk( Long );
static	int	mem_wrt_chk( Long );
void	run68_abort( Long );

/*
 　機能：PCの指すメモリからインデックスレジスタ＋8ビットディスプレースメント
 　　　　の値を得る
 戻り値：その値
*/
Long idx_get(BOOL* err)
{
	unsigned char	*mem;
	char	idx2;
	char	idx_reg;
	char	scale;
	Long	idx;
    Long    disp = 0;

	mem = prog_ptr + pc;
	idx2 = *(mem++);
	idx_reg = ((idx2 >> 4) & 0x07);
	if ( (idx2 & 0x80) == 0 )
		idx = rd [ idx_reg ];
	else
		idx = ra [ idx_reg ];
	if ( (idx2 & 0x08) == 0 ) {	/* WORD */
		if ((idx & 0x8000) != 0)
			idx |= 0xFFFF0000;
		else
			idx &= 0x0000FFFF;
	}
	scale = 1 << ((idx2 & 0x06) >> 1);  // 030拡張: 1,2,4,8倍のスケールファクタ
	pc += 2;
    if ( (idx2 & 0x01) == 0 ) {
        disp = *mem;
    } else {
        Long idx3 = *(mem++);
        // 030拡張: フルフォーマットの間接指定
        BOOL bs = (idx3 >> 7) & 0x01;
        BOOL is = (idx3 >> 6) & 0x01;
        int bd_size = (idx3 >> 4) & 0x03;
        BOOL resv = (idx3 >> 3) & 0x01;
        int iis = (idx3) & 0x07;
        if (bs) {
            // ベースレジスタサプレスは未サポート
            *err =TRUE;
            return 0;
        }
        switch ( bd_size ) { // MC68030ユーザーズマニュアル(日本語) P31参照
            case 1: // ヌルディスプレースメント
                disp = 0; // あってる？
                break;
            case 2: // ワードディスプレースメント
                pc += 2;
                disp = *(mem++);
                disp = ((disp << 8) | *mem);
                break;
            case 3: // ロングワードディスプレースメント
                pc += 4;
                disp = *(mem++);
                disp = ((disp << 8) | *(mem++));
                disp = ((disp << 8) | *(mem++));
                disp = ((disp << 8) | *mem);
                break;
            default:
                *err =TRUE;
                return 0;
        }
        if (resv) { // 予約ビットが1の場合解釈不能
            *err =TRUE;
            return 0;
        }
        switch ( is ? (8+iis) : (iis)) { // MC68030ユーザーズマニュアル(日本語) P32参照
            case 0:     // メモリ間接なし
                break;
            default:
                // アウタディスプレースメントはまだ未対応
                *err =TRUE;
                return 0;
        }
    }

	return( (idx * scale) + disp );
}

/*
 　機能：PCの指すメモリから指定されたサイズのイミディエイトデータをゲットし、
 　　　　サイズに応じてPCを進める
 戻り値：データの値
*/
Long imi_get( char size )
{
	UChar	*mem;
	Long	d;

	mem = (UChar *)prog_ptr + pc;

	switch( size ) {
		case S_BYTE:
			pc += 2;
			return( *(mem + 1) );
		case S_WORD:
			pc += 2;
			d = *(mem++);
			d = ((d << 8) | *mem);
			return( d );
		default:	/* S_LONG */
			pc += 4;
			d = *(mem++);
			d = ((d << 8) | *(mem++));
			d = ((d << 8) | *(mem++));
			d = ((d << 8) | *mem);
			return( d );
	}
}

/*
 　機能：メモリから指定されたサイズのデータをゲットする
 戻り値：データの値
*/
Long mem_get( Long adr, char size )
{
	UChar   *mem;
	Long	d;

	if ( adr < ENV_TOP || adr >= mem_aloc ) {
		if ( mem_red_chk( adr ) == FALSE )
			return( 0 );
	}
	mem = (UChar *)prog_ptr + adr;

	switch( size ) {
		case S_BYTE:
			return( *mem );
		case S_WORD:
			d = *(mem++);
			d = ((d << 8) | *mem);
			return( d );
		default:	/* S_LONG */
			d = *(mem++);
			d = ((d << 8) | *(mem++));
			d = ((d << 8) | *(mem++));
			d = ((d << 8) | *mem);
			return( d );
	}
}

/*
 　機能：メモリに指定されたサイズのデータをセットする
 戻り値：なし
*/
void mem_set( Long adr, Long d, char size )
{
	UChar   *mem;

	if ( adr < ENV_TOP || adr >= mem_aloc ) {
		if ( mem_wrt_chk( adr ) == FALSE )
			return;
	}
	mem = (UChar *)prog_ptr + adr;

	switch( size ) {
		case S_BYTE:
			*mem = (d & 0xFF);
			return;
		case S_WORD:
			*(mem++) = ((d >> 8) & 0xFF);
			*mem = (d & 0xFF);
			return;
		default:	/* S_LONG */
			*(mem++) = ((d >> 24) & 0xFF);
			*(mem++) = ((d >> 16) & 0xFF);
			*(mem++) = ((d >> 8) & 0xFF);
			*mem = (d & 0xFF);
			return;
	}
}

/*
 　機能：読み込みアドレスのチェック
 戻り値： TRUE = OK
         FALSE = NGだが、0を読み込んだとみなす
*/
static int mem_red_chk( Long adr )
{
	char message[256];

	adr &= 0x00FFFFFF;
	if ( adr >= 0xC00000 ) {
		if ( ini_info.io_through == TRUE )
			return( FALSE );
		sprintf(message, "I/OポートorROM($%06X)から読み込もうとしました。", adr);
		err68(message);
		run68_abort( adr );
	}
	if ( SR_S_REF() == 0 || adr >= mem_aloc ) {
		sprintf(message, "不正アドレス($%06X)からの読み込みです。", adr);
		err68(message);
		run68_abort( adr );
	}
	return( TRUE );
}

/*
 　機能：書き込みアドレスのチェック
 戻り値： TRUE = OK
         FALSE = NGだが、何も書き込まずにOKとみなす
*/
static int mem_wrt_chk( Long adr )
{
	char message[256];

	adr &= 0x00FFFFFF;
	if ( adr >= 0xC00000 ) {
		if ( ini_info.io_through == TRUE )
			return( FALSE );
/*
		if ( adr == 0xE8A01F )	/# RESET CONTROLLER #/
			return( FALSE );
*/
		sprintf(message, "I/OポートorROM($%06X)に書き込もうとしました。", adr);
		err68(message);
		run68_abort(adr);
	}
	if ( SR_S_REF() == 0 || adr >= mem_aloc ) {
		sprintf(message, "不正アドレスへの書き込みです($%06X)", adr);
		err68(message);
		run68_abort( adr );
	}
	return( TRUE );
}

/*
 機能：異常終了する
*/
void run68_abort( Long adr )
{
	int	i;

	fprintf( stderr, "アドレス：%08X\n", adr );

	for ( i = 5; i < FILE_MAX; i ++ ) {
		if ( finfo [ i ].fh != NULL )
#if defined(WIN32)
			CloseHandle(finfo [ i ].fh);
#else
			fclose(finfo [ i ].fh);
#endif
	}

#ifdef	TRACE
	printf( "d0-7=%08lx" , rd [ 0 ] );
	for ( i = 1; i < 8; i++ ) {
		printf( ",%08lx" , rd [ i ] );
	}
	printf("\n");
	printf( "a0-7=%08lx" , ra [ 0 ] );
	for ( i = 1; i < 8; i++ ) {
		printf( ",%08lx" , ra [ i ] );
	}
	printf("\n");
	printf( "  pc=%08lx    sr=%04x\n" , pc, sr );
#endif
	longjmp(jmp_when_abort, 2);
}
