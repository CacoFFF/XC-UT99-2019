/*=============================================================================
	XC_Template.h

	Additional templates for XC_Core.
=============================================================================*/

#ifndef UNXC_TEMPLATE_H
#define UNXC_TEMPLATE_H

#pragma once

//Specialized lightweight helpers
template<class T> inline void ExchangeRaw( T& A, T& B )
{
	register INT Tmp;
	for ( INT i=0 ; i<sizeof(T)/sizeof(Tmp) ; i++ )
	{
		Tmp = ((INT*)&A)[i];
		((INT*)&A)[i] = ((INT*)&B)[i];
		((INT*)&B)[i] = Tmp;
	}
}

template < INT cmpsize > inline INT appStrncmp( const TCHAR* S1, const TCHAR(&S2)[cmpsize])
{
	return appStrncmp( S1, S2, cmpsize-1);
}


#endif
