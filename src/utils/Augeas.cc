/*---------------------------------------------------------------------------*\
                          ____  _ _ __ _ __  ___ _ _
                         |_ / || | '_ \ '_ \/ -_) '_|
                         /__|\_, | .__/ .__/\___|_|
                             |__/|_|  |_|
\*---------------------------------------------------------------------------*/
#include <iostream>
#include <stdlib.h>

#include <zypp/base/Logger.h>
#include <zypp/Pathname.h>

#include "Zypper.h"
#include "utils/Augeas.h"

Augeas::Augeas( const std::string & file )
: _augeas( NULL )
, _got_global_zypper_conf( false )
, _got_user_zypper_conf( false )
{
  MIL << "Going to read zypper config using Augeas..." << file << endl;

  // init
  _augeas = ::aug_init( NULL, "/usr/share/zypper", AUG_NO_STDINC | AUG_NO_LOAD );
  if (_augeas == NULL)
    ZYPP_THROW(Exception(_("Cannot initialize configuration file parser.")));

  // tell augeas which file to load
  // if file is specified, try to load the file, fall back to default files
  // default - /etc/zypp/zypper.conf & $HOME/.zypper.conf
  Pathname filepath( file );
  bool want_custom = false;
  if ( !file.empty() && PathInfo(filepath).isExist() )
  {
    want_custom = true;
    if ( filepath.relative() )
    {
      const char * env = ::getenv("PWD");
      std::string wd( env ? env : "." );
      filepath = wd / filepath;
    }
MIL << "/augeas/load/ZYpper/incl" / filepath << endl;
    if ( ::aug_set( _augeas, "/augeas/load/ZYpper/incl", filepath.asString().c_str() ) != 0 )
      ZYPP_THROW(Exception(_("Augeas error: setting config file to load failed.")));
  }
  else
  {
    const char * env = ::getenv("HOME");
    if ( env )
      _homedir = env;
    if ( _homedir.empty() )
      WAR << "Cannot figure out user's home directory. Skipping user's config." << endl;
    else
    {
      // add $HOME/.zypper.conf; /etc/zypp/zypper.conf should already be included
      filepath = _homedir + "/.zypper.conf";
MIL << "/augeas/load/ZYpper/incl[2]" / filepath << endl;
      if ( ::aug_set( _augeas, "/augeas/load/ZYpper/incl[2]", filepath.asString().c_str() ) != 0 )
        ZYPP_THROW(Exception(_("Augeas error: setting config file to load failed.")));
    }
  }

  // load the file
  if ( aug_load( _augeas ) != 0 )
    ZYPP_THROW(Exception(_("Could not parse the config files.")));

  // collect eventual errors
  const char *value[1] = {};
  std::string error;
  if ( want_custom )
    _user_conf_path = "/files" + filepath.asString();
  else
  {
    if ( !_homedir.empty() )
      _user_conf_path = "/files" + _homedir + "/.zypper.conf";

    // global conf errors
    _got_global_zypper_conf = ::aug_get(_augeas, "/files/etc/zypp/zypper.conf", NULL) != 0;
    if ( ::aug_get( _augeas, "/augeas/files/etc/zypp/zypper.conf/error/message", value ) )
      error = std::string("/etc/zypp/zypper.conf: ") + value[0];
  }

  // user conf errors
  if ( !_user_conf_path.empty() )
  {
    _got_user_zypper_conf = ::aug_get( _augeas, _user_conf_path.c_str(), NULL ) != 0;
    std::string user_conf_err( "/augeas" + _user_conf_path + "/error/message" );
    if ( ::aug_get( _augeas, user_conf_err.c_str(), value ) )
    {
      if ( ! error.empty() )
	error += "\n";
      error += _user_conf_path.substr( 6 );
      error += ": ";
      error += value[0];
    }
  }

  if ( !_got_global_zypper_conf && !_got_user_zypper_conf && !error.empty() )
  {
    std::string msg(_("Error parsing zypper.conf:") + std::string("\n") + error);
    ZYPP_THROW(Exception(msg));
  }

  MIL << "Done reading conf files:" << endl;
  if (want_custom)
    MIL << "custom conf read: " << (_got_user_zypper_conf ? "yes" : "no") << endl;
  else
  {
    MIL << "user conf read: " << (_got_user_zypper_conf ? "yes" : "no") << endl;
    MIL << "global conf read: " << (_got_global_zypper_conf ? "yes" : "no") << endl;
  }
}

// ---------------------------------------------------------------------------

Augeas::~Augeas()
{
  if ( _augeas != NULL )
    ::aug_close( _augeas );
}

// ---------------------------------------------------------------------------

static std::string global_option_path( const std::string & section, const std::string & option )
{
  return "/files/etc/zypp/zypper.conf/" + section + "/*/" + option;
}

std::string Augeas::userOptionPath( const std::string & section, const std::string & option ) const
{
  if ( _user_conf_path.empty() )
    return "(empty)";
  return _user_conf_path + "/" + section + "/*/" + option;
}

// ---------------------------------------------------------------------------

std::string Augeas::get( const std::string & augpath ) const
{
  const char *value[1] = {};
  _last_get_result = ::aug_get( _augeas, augpath.c_str(), value );
  if ( _last_get_result == 1 )
  {
    DBG << "Got " << augpath << " = " << value[0] << endl;
    return value[0];
  }
  else if ( _last_get_result > 1 )
    WAR << "Multiple matches for " << augpath << endl;

  return std::string();
}

// ---------------------------------------------------------------------------

std::string Augeas::getOption( const std::string & option ) const
{
  std::vector<std::string> opt;
  str::split( option, back_inserter(opt), "/" );

  if ( opt.size() != 2 || opt[0].empty() || opt[1].empty() )
  {
    ERR << "invalid option " << option << endl;
    return std::string();
  }

  if ( _got_user_zypper_conf )
  {
    std::string augpath_u( userOptionPath( opt[0], opt[1] ) );
    std::string result( get( augpath_u ) );
    if ( _last_get_result == 1 )
      return result;
  }

  if ( _got_global_zypper_conf )
  {
    std::string augpath_g( global_option_path( opt[0], opt[1] ) );
    std::string result( get( augpath_g ) );
    if ( _last_get_result == 1 )
      return result;
  }

  return std::string();
}

///////////////////////////////////////////////////////////////////
namespace
{
  ///////////////////////////////////////////////////////////////////
  struct AugRef
  {
    AugRef( const ::augeas & aug_r )
    : _aug { aug_r }
    {}

    const augeas *const aug() const
    { return &_aug; }

//     augeas *const aug()
//     { return &_aug; }

  private:
    const ::augeas & _aug;
  };

  ///////////////////////////////////////////////////////////////////
  struct AugPath : public AugRef, public std::string
  {
    AugPath( const AugRef & aug_r, std::string path_r = std::string() )
    : AugRef { aug_r }
    , std::string { std::move(path_r) }
    {}

    std::string label() const
    {
      const char * v { nullptr };
      if ( ::aug_label( aug(), c_str(), &v ) == 1 )
	return v;
      throw std::domain_error( std::string("Bad path ")+*this );
    }

    std::pair<std::string,bool> valueIf() const
    {
      const char * v { nullptr };
      if ( ::aug_get( aug(), c_str(), &v ) == 1 )
	return { v ? v : "", bool(v) };
      throw std::domain_error( std::string("Bad path ")+*this );
    }

    bool hasvalue() const
    { return valueIf().second; }

    std::string value() const
    { return valueIf().first; }
  };


  ///////////////////////////////////////////////////////////////////
  struct AugMatch : public AugRef
  {
    AugMatch( const AugRef & aug_r, const char * path_r )
    : AugRef { aug_r }
    {
      int res = ::aug_match( aug(), path_r, &_matches );
      if ( res == -1 )
	throw std::domain_error( std::string("Bad path ")+path_r );
      _cnt = res;
    }

    AugMatch( const AugRef & aug_r, const std::string & path_r )
    : AugMatch { aug_r, path_r.c_str() }
    {}

    AugMatch( const AugPath & path_r )
    : AugMatch { path_r, path_r.c_str() }
    {}

    ~AugMatch()
    {
      if ( _matches )
      {
	while ( _cnt-- )
	  ::free( _matches[_cnt] );
	::free( _matches );
      }
    }

  private:
    struct Stringify : public AugRef
    {
      Stringify( const AugRef & aug_r )
      : AugRef { aug_r }
      {}

      typedef AugPath result_type;
      result_type operator()( char * el_r ) const
      { return { *this, el_r }; }
    };

  public:
    typedef Stringify::result_type value_type;

    typedef transform_iterator<Stringify,char**> const_iterator;

    bool empty() const
    { return _cnt; }

    unsigned size() const
    { return _cnt; }

    const_iterator begin() const
    { return make_transform_iterator( _matches, Stringify( *this ) ); }

    const_iterator end() const
    { return make_transform_iterator( _matches+_cnt, Stringify( *this ) ); }

    value_type operator[]( unsigned idx_r ) const
    {
      if ( idx_r < _cnt )
	return { *this, _matches[idx_r] };
      throw std::out_of_range( "Bad index" );
    }

  private:
    unsigned		_cnt	{ 0U };
    char **		_matches{ nullptr };
  };

  ///////////////////////////////////////////////////////////////////
  struct DumpAugeas
  {
    DumpAugeas( const ::augeas & aug_r )
    : _aug  { aug_r }
    {}

    std::ostream & dumpOn( std::ostream & str_r ) const
    { return recDumpOn( str_r, "/" ); }

  private:
    std::ostream & recDumpOn( std::ostream & str_r, const std::string & path_r ) const
    {
      for ( const auto & n : AugMatch( _aug, path_r) )
      {
	str_r
	<< "  " << n
	<< " -{" << n.label() << "}- "
	<< " -" << ( n.hasvalue() ? "\""+n.value()+"\"" : "" ) << "- "
	<< endl;
	recDumpOn( str_r, n+"/*" );
      }
      return str_r;

    }

  private:
    const ::augeas & _aug;
  };

  inline std::ostream & operator<<( std::ostream & str_r, const DumpAugeas & obj_r )
  { return obj_r.dumpOn( str_r ); }

} // namespace
///////////////////////////////////////////////////////////////////

std::ostream & operator<<( std::ostream & str_r, const Augeas & obj_r )
{
  if ( ::augeas * aug = obj_r._augeas )
    str_r << "Augeas {" << endl << DumpAugeas(*aug) << "}";
  else
    str_r << "Augeas {}";

  return str_r;
}
