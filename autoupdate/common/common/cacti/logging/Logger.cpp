#include "stdafx.h"

#include <cacti/logging/Logger.h>

#include <cacti/logging/LogManager.h>

#include <cacti/util/StringUtil.h>

#define new DEBUG_NEW

namespace cacti
{
    Logger& Logger::getRoot() 
	{
        return getInstance("");
    }

    void Logger::setRootLevel(const LogControlID& level) 
	{
        getRoot().setLevel(level);
    }

    LogControlID Logger::getRootLevel() throw() 
	{
        return getRoot().getLevel();
    }

    Logger& Logger::getInstance(const std::string& name) 
	{
        return LogManager::getDefaultManager().getInstance(name);
    }

    void Logger::shutdown()
	{
        LogManager::shutdown();
    }

    Logger::Logger(const std::string& name, LoggerPtr parent, const LogControlID& level) 
		: m_name(name)
		, m_parent(parent)
		, m_level(level)
		, m_isAdditive(false)
	{
    }

    Logger::~Logger() 
	{
    }

    const std::string& Logger::getName() const throw() 
	{
        return m_name; 
    }
    
    LogControlID Logger::getLevel() const throw() 
	{ 
        return m_level; 
    }

    void Logger::setLevel(const LogControlID& level)
	{
        if ((level < LogLevel::NOTSET) || (getParent() != 0)) 
		{
            m_level = level;
        } 
		else 
		{
            /* caller tried to set NOTSET level to root Logger. 
               Bad caller!
            */
            throw std::invalid_argument("cannot set level NOTSET on Root Logger");
        }
    }
    
    LogControlID Logger::getChainedLevel() const throw() 
	{
		// REQUIRE(root->getLevel() != LogLevel::NOTSET)
        const Logger* c = this;
        while(c->getLevel() >= LogLevel::NOTSET) { 
            c = c->getParent().get();
        }
        
        return c->getLevel();
    }
    
	bool Logger::isLevelEnabled(const LogControlID& level) const throw() 
	{
		return(getChainedLevel() >= level);
	}

    void Logger::addHandler(LogHandlerPtr handler)
	{
        if (handler) 
		{
            RecursiveMutex::ScopedLock lock(m_handlerSetMutex);
            {
				std::list<LogHandlerPtr>::iterator it = 
					std::find(m_handlers.begin(), m_handlers.end(), handler);

                if (it == m_handlers.end()) 
				{
                    // not found
                    m_handlers.push_back(handler);
                }
            }
        } 
		else 
		{
            throw std::invalid_argument("NULL handler");
        }
    }
    
	std::list<LogHandlerPtr> Logger::getAllHandlers()
	{
		{ // lock begin
			RecursiveMutex::ScopedLock lock(m_handlerSetMutex);
			return m_handlers;
		} // lock end
	}

	LogHandlerPtr Logger::getHandler(const std::string& name)
	{
		{ // lock begin
			RecursiveMutex::ScopedLock lock(m_handlerSetMutex);
			std::list<LogHandlerPtr>::iterator it = m_handlers.begin();
			std::list<LogHandlerPtr>::iterator endit = m_handlers.end();
			while(it != endit)
			{
				if((*it)->getName() == name)
					return *it;

				++it;
			}
			return LogHandlerPtr((LogHandler*)0);
		} // lock end
	}

	void Logger::removeAllHandlers()
	{
		{ // lock 
			RecursiveMutex::ScopedLock lock(m_handlerSetMutex);
			m_handlers.erase(m_handlers.begin(), m_handlers.end());
		} // unlock
	}

	void Logger::removeHandler(LogHandlerPtr handler)
	{
		if(handler)
		{
			{ // lock
				RecursiveMutex::ScopedLock lock(m_handlerSetMutex);
				std::list<LogHandlerPtr>::iterator it = 
					std::find(m_handlers.begin(), m_handlers.end(), handler);

				if (it != m_handlers.end()) 
				{
					m_handlers.erase(it);
				}
			} // unlock
		}
	}

	void Logger::removeHandler(const std::string& name)
	{
		removeHandler(getHandler(name));
	}

    void Logger::callHandlers(const LogRecord& rec) throw() 
	{
        { // lock
			RecursiveMutex::ScopedLock lock(m_handlerSetMutex);
            if (!m_handlers.empty()) 
			{
                for(std::list<LogHandlerPtr>::iterator it = m_handlers.begin(); 
					it != m_handlers.end(); it++) 
				{
                    (*it)->publish(rec);
                }
            }
			if (getAdditivity() && (getParent() != 0)) 
			{
				getParent()->callHandlers(rec);
			}
		} // unlock
    }

    void Logger::setAdditivity(bool additivity) 
	{
        m_isAdditive = additivity;
    }

    bool Logger::getAdditivity() const throw() 
	{
        return m_isAdditive; 
    }

    LoggerPtr Logger::getParent() throw() 
	{
        return m_parent; 
    }

    const LoggerPtr Logger::getParent() const throw() 
	{
        return m_parent; 
    }

    void Logger::logUnconditionally(const LogControlID& level, 
                                    const char* format, 
                                    va_list arguments) throw() 
	{
#ifndef _DEBUG
		try {
#endif
			
			LogRecord event(getName(), format, arguments, level);
			callHandlers(event);

#ifndef _DEBUG
		}catch(...)
		{
			LogRecord event(getName(), format, LogLevel::FATAL);
			callHandlers(event);
		}
#endif
    }
    
	void Logger::logUnconditionally(const LogControlID& level, 
		unsigned int index,
		const char* format, 
		va_list arguments) throw() 
	{
#ifndef _DEBUG
		try {
#endif

			LogRecord event(getName(), format, arguments, level, true, index);
			callHandlers(event);

#ifndef _DEBUG
		}catch(...)
		{
			LogRecord event(getName(), format, LogLevel::FATAL, true, index);
			callHandlers(event);
		}
#endif
	}

	void Logger::logUnconditionallyNoTime(const LogControlID& level, 
		const char* format, 
		va_list arguments) throw()
	{
#ifndef _DEBUG
		try {
#endif

			LogRecord event(getName(), format, arguments, level, false, 0, Timestamp(0, 0));
			callHandlers(event);

#ifndef _DEBUG
		}catch(...)
		{
			LogRecord event(getName(), format, level, false, 0, Timestamp(0, 0));
			callHandlers(event);
		}
#endif
	}

	void Logger::log(const LogControlID& level,
					 const char* stringFormat, ...) throw() 
	{
		if (isLevelEnabled(level)) 
		{
			va_list va;
			va_start(va, stringFormat);
			logUnconditionally(level, stringFormat, va);
			va_end(va);
		}
	}
	void Logger::log(const LogControlID& level,
		unsigned int index,
		const char* stringFormat, ...) throw() 
	{
		if (isLevelEnabled(level)) 
		{
			va_list va;
			va_start(va, stringFormat);
			logUnconditionally(level, index, stringFormat, va);
			va_end(va);
		}
	}

	void Logger::log(const LogControlID& level, const boost::format& bf) throw()
	{
		if(isLevelEnabled(level))
		{
			// we don't want any exception, so we close all exception bit
			const_cast<boost::format&>(bf).exceptions(boost::io::no_error_bits);
			LogRecord event(getName(), bf.str(), level);
			callHandlers(event);
		}
	}

	void Logger::log(const LogControlID& level, unsigned int index, const boost::format& bf) throw()
	{
		if(isLevelEnabled(level))
		{
			// we don't want any exception, so we close all exception bit
			const_cast<boost::format&>(bf).exceptions(boost::io::no_error_bits);
			LogRecord event(getName(), bf.str(), level, true, index);
			callHandlers(event);
		}
	}


	void Logger::logva(const LogControlID& level, 
					   const char* stringFormat,
					   va_list va) throw() 
	{ 
		if (isLevelEnabled(level)) 
		{
			logUnconditionally(level, stringFormat, va);
		}
	}
	void Logger::logva(const LogControlID& level, 
		unsigned int index, 
		const char* stringFormat,
		va_list va) throw() 
	{ 
		if (isLevelEnabled(level)) 
		{
			logUnconditionally(level, index, stringFormat, va);
		}
	}
	void Logger::logvaNoTime(const LogControlID& level,
		const char* stringFormat,
		va_list va) throw()
	{
		if(isLevelEnabled(level))
		{
			logUnconditionallyNoTime(level, stringFormat, va);
		}
	}
	void Logger::log(const LogControlID& level, 
					 const std::string& message) throw() 
	{ 
        if (isLevelEnabled(level))
		{
			LogRecord event(getName(), message, level);
			callHandlers(event);
		}
    }

	void Logger::log(const LogControlID& level, 
		unsigned int index, 
		const std::string& message) throw() 
	{ 
		if (isLevelEnabled(level))
		{
			LogRecord event(getName(), message, level, true, index);
			callHandlers(event);
		}
	}

	void Logger::logNoTime(const LogControlID& level, 
		const std::string& message) throw()
	{
		if (isLevelEnabled(level))
			logUnconditionallyNoTime(level, message.c_str(), NULL);
	}

	void Logger::debug(const char* stringFormat, ...) throw() 
	{ 
        if (isLevelEnabled(LogLevel::DBG)) 
		{
            va_list va;
            va_start(va,stringFormat);
            logUnconditionally(LogLevel::DBG, stringFormat, va);
            va_end(va);
        }
    }
    
    void Logger::debug(const std::string& message) throw() 
	{ 
        if (isLevelEnabled(LogLevel::DBG))
		{
			LogRecord event(getName(), message, LogLevel::DBG);
			callHandlers(event);
		}
    }
    
    void Logger::info(const char* stringFormat, ...) throw() 
	{ 
//         if (isLevelEnabled(LogLevel::INFO)) 
// 		{
//             va_list va;
//             va_start(va,stringFormat);
//             logUnconditionally(LogLevel::INFO, stringFormat, va);
//             va_end(va);
//         }
    }
    
    void Logger::info(const std::string& message) throw() 
	{ 
        if (isLevelEnabled(LogLevel::INFO))
		{
			LogRecord event(getName(), message, LogLevel::INFO);
			callHandlers(event);
		}
    }
    
    void Logger::warn(const char* stringFormat, ...) throw() 
	{ 
        if (isLevelEnabled(LogLevel::WARN)) 
		{
            va_list va;
            va_start(va,stringFormat);
            logUnconditionally(LogLevel::WARN, stringFormat, va);
            va_end(va);
        }
    }
    
    void Logger::warn(const std::string& message) throw() 
	{ 
        if (isLevelEnabled(LogLevel::WARN))
		{
			LogRecord event(getName(), message, LogLevel::WARN);
			callHandlers(event);   
		}
    }
    
    void Logger::error(const char* stringFormat, ...) throw() 
	{ 
        if (isLevelEnabled(LogLevel::ERR)) 
		{
            va_list va;
            va_start(va,stringFormat);
			logUnconditionally(LogLevel::ERR, stringFormat, va);
            va_end(va);
        }
    }
    
    void Logger::error(const std::string& message) throw() 
	{ 
        if (isLevelEnabled(LogLevel::ERR))
		{
			LogRecord event(getName(), message, LogLevel::ERR);
			callHandlers(event);   
		}
    }

    void Logger::alert(const char* stringFormat, ...) throw() 
	{ 
        if (isLevelEnabled(LogLevel::ALERT)) 
		{
            va_list va;
            va_start(va,stringFormat);
            logUnconditionally(LogLevel::ALERT, stringFormat, va);
            va_end(va);
        }
    }
    
    void Logger::alert(const std::string& message) throw() 
	{ 
        if (isLevelEnabled(LogLevel::ALERT))
		{
			LogRecord event(getName(), message, LogLevel::ALERT);
			callHandlers(event);   
		}
    }

    void Logger::fatal(const char* stringFormat, ...) throw() 
	{ 
        if (isLevelEnabled(LogLevel::FATAL)) 
		{
            va_list va;
            va_start(va,stringFormat);
            logUnconditionally(LogLevel::FATAL, stringFormat, va);
            va_end(va);
        }
    }
    
    void Logger::fatal(const std::string& message) throw() 
	{ 
        if (isLevelEnabled(LogLevel::FATAL))
		{
			LogRecord event(getName(), message, LogLevel::FATAL);
			callHandlers(event);
		}
    }

    LoggerStream Logger::getStream(const LogControlID& level) 
	{
        return LoggerStream(*this, isLevelEnabled(level) ?
                              level : LogLevel::NOTSET);
    }
} 

