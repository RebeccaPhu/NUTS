#include "stdafx.h"
#include "IconRatio.h"

/* This functon recalculated the dimensions of an icon according to several factors:

   * The dimensions of the original screen mode, as mapped onto 4:3 display.
   * The dimensions of the icon being displayed
   * The limits of the space the icon can be displayed in.
*/
AspectRatio ScreenCompensatedIconRatio( AspectRatio Icon, AspectRatio Screen, WORD MaxWidth, WORD MaxHeight )
{
	/* Step one, let's consider the screen resolution. We can work out the co-efficients from what
	   the screen *should* look like if the world was in order. This requires considering whether,
	   pixelwise, the screen is wider or taller.
	*/

	double XMultiplier = 1.0;
	double YMultiplier = 1.0;

	if ( Screen.first >= Screen.second )
	{
		/* Screen is pixelwise wider than it is tall */
		double sw  = Screen.first;
               sw /= 4.0;
			   sw *= 3.0;

		YMultiplier = sw / (double) Screen.second;
	}
	else if ( Screen.second > Screen.first )
	{
		/* Screen is pixelwise taller than it is wide */
		double sh  = Screen.second;
               sh /= 3.0;
			   sh *= 4.0;

		XMultiplier = sh / (double) Screen.second;
	}
	
	/* Note: If dimensions are equal, then it is treated as if it is wider than tall, as per the 4:3 aspect */

	/* Step two: Multiply the icon dimensions by the agreed multipliers. If all is good, one of these should be 1.0 */

	double NewIconWidth  = (double) Icon.first  * XMultiplier;
	double NewIconHeight = (double) Icon.second * YMultiplier;

	/* Step three: This would normally be enough, but now we'll examine these dimensions, and rescale them according to limits */
	while ( ( NewIconWidth > MaxWidth ) || ( NewIconHeight > MaxHeight ) )
	{
		if ( NewIconWidth > MaxWidth )
		{
			double iwr = (double) MaxWidth / NewIconWidth;

			NewIconWidth  *= iwr;
			NewIconHeight *= iwr;
		}

		if ( NewIconHeight > MaxHeight )
		{
			double ihr = (double) MaxHeight / NewIconHeight;

			NewIconHeight *= ihr;
			NewIconWidth  *= ihr;
		}
	}

	return AspectRatio( (WORD) NewIconWidth, (WORD) NewIconHeight );
}
