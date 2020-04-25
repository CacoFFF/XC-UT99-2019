/*=============================================================================
	XC_Template.h

	Additional templates for XC_Core.
=============================================================================*/

#ifndef UNXC_TEMPLATE_H
#define UNXC_TEMPLATE_H

#pragma once

//Specialized lightweight helpers

/** ExchangeRaw
 *
 * Exchanges objects of any kind using plain CPU registers
 *
 * This is extremely useful when it comes to reducing amount of malloc/free operations.
 * It can also be used to somewhat imitate move semantics without explicitly using any of those features.
*/

//Do not call this directly
template <typename E,typename T> FORCEINLINE void ExchangeRawUsing( T& A, T& B)
{
	E* pA = (E*)&A;
	E* pB = (E*)&B;
	E Tmp;
	for ( int i=0 ; i<sizeof(T)/sizeof(E) ; i++ )
	{
		Tmp = *pA;
		*pA++ = *pB;
		*pB++ = Tmp;
	}
}

template <class T> inline void ExchangeRaw( T& A, T& B )
{
#if defined(_M_IX86) || defined(_M_X64)
	if ( sizeof(T) % sizeof(__m128) == 0 ) //Use XMMWORD registers (code necessary to force MOVUPS instead of MOVAPS) //TODO: ADD LINUX LATER
	{
		float* pA = (float*)&A;
		float* pB = (float*)&B;
		__m128 Tmp;
		for ( int i=0 ; i<sizeof(T)/sizeof(__m128) ; i++ )
		{
			Tmp = _mm_loadu_ps( &pA[i*4]);
			_mm_storeu_ps( &pA[i*4], _mm_loadu_ps(&pB[i*4]));
			_mm_storeu_ps( &pB[i*4], Tmp);
		}
	}
	else
#endif
	if ( sizeof(T) % sizeof(PTRINT) == 0 ) //Optional 8 byte exchange for 64 bit builds
		ExchangeRawUsing<PTRINT>( A, B);
	else 
	if ( sizeof(T) % sizeof(int) == 0 ) //Default int exchange mode
		ExchangeRawUsing<int>( A, B);
	else
		ExchangeRawUsing<char>( A, B);
}

template <typename T> inline void InvertArray( TArray<T>& A)
{
	const INT Num = A.Num();
	const INT Half = Num / 2;
	for ( INT i=0 ; i<Half ; i++ )
		ExchangeRaw( A(i), A(Num-(i+1)) );
}

template < INT cmpsize > inline INT appStrncmp( const TCHAR* S1, const TCHAR(&S2)[cmpsize])
{
	return appStrncmp( S1, S2, cmpsize-1);
}

template < INT cmpsize > inline INT appStrnicmp( const TCHAR* S1, const TCHAR(&S2)[cmpsize])
{
	return appStrnicmp( S1, S2, cmpsize-1);
}
#endif
