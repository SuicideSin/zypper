/*---------------------------------------------------------------------------*\
                          ____  _ _ __ _ __  ___ _ _
                         |_ / || | '_ \ '_ \/ -_) '_|
                         /__|\_, | .__/ .__/\___|_|
                             |__/|_|  |_|
\*---------------------------------------------------------------------------*/
#ifndef ZYPPER_@COMMAND@_H
#define ZYPPER_@COMMAND@_H

#include <string>
class Zypper;

/*
 @command@ ...
*/

/** @command@ specific options */
struct @Command@Options : public MixinOptions<ZypperCommand::@COMMAND@>
{
  @Command@Options()
  {}

  //** @Command@ user help (translated). */
  //virtual std::ostream & showHelpOn( std::ostream & out ) const;

  bool	_myopt = true;	//< opts go here...
};

/** Execute @command@.
 */
int @command@( Zypper & zypper_r );

#endif // ZYPPER_@COMMAND@_H
