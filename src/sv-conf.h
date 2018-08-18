/*
 * Copyright (c) 2016-2018 tokiclover <tokiclover@gmail.com>
 * This file is part of Supervision
 *
 * Supervision is free software; you can redistribute
 * it and/or modify it under the terms of the 2-clause, simplified,
 * new BSD License included in the distriution of this package.
 *
 * @(#)sv-conf.h  0.14.0 2018/08/18
 */

#ifndef SV_CONF_H
#define SV_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

extern int sv_debug;

/*
 * retrieve a configuration value like getenv(3)
 * @envp: configuration list;
 * @env: configuration name;
 */
extern const char *sv_getconf(const char *env);
/*
 * simple helper to ease yes/no querries of config settings
 * @return: true/false;
 */
extern int sv_conf_yesno(const char *env);

#ifdef __cplusplus
}
#endif

#endif /* SV_CONF_H */
