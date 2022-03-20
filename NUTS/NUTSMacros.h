#ifndef NUTSMACROS_H
#define NUTSMACROS_H

#define NixWindow( W ) if ( W != nullptr ) { DestroyWindow( W ); W = nullptr; }
#define NixObject( O ) if ( O != NULL ) { DeleteObject( O ); O = NULL; }

#endif
