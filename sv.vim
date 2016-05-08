"
" Language:	Supervision Scripts Framework
" Licence:	Distributed under the same terms as Vim itself and/or 2-clause BSD
"
" Syntax highlighting for Supervision Scripts Framework by inheriting sh.vim.
" Requires vim 6.3 or later.
"

if &compatible || v:version < 603
    finish
endif

if exists("b:current_syntax")
    finish
endif

let is_bash=1
runtime! syntax/sh.vim
unlet b:current_syntax

syn keyword svKwd begin die end error eval_colors info yesno checkpath warn SOURCE
syn keyword svKwd env_svc env_sv  svc_cmd svc_down svc_env svc_mark svc_state
syn keyword svKwd env_rs rs_cmd svc_wait svc_zap mount_info
syn keyword svVars SVC_DEBUG SVC_CONFIGFILE SVC_CMD SVC_OPTS SVC_GROUP SVC_USER
syn keyword svVars SVC_USE SVC_NEED SVC_BEFORE SVC_AFTER SVC_TRY SV_TRY SV_TERM
syn keyword svVars ENV_DIR ENV_CMD ENV_OPTS PRE_CMD PRE_OPTS FIN_CMD FIN_OPTS
syn keyword svVars LOG_CMD LOG_OPTS LOG_FIN_CMD LOG_FIN_OPTS SVC_NEED RC_OPTS
syn keyword svVars CGROUP_CLEANUP CGROUP_INHERIT SV_CGROUP SVC_DEPS SVC_PIDFILE
syn keyword svVars NULL SV_SVCDIR SV_TMPDIR SV_RUNDIR SV_SERVICE SV_OPT SV_CMD
syn keyword svVars LOGDIR LOGOPT LOG_STAT LOG_COMP LOG_PREFIX LOG_SIZE SVC_COMMANDS
syn keyword svVars description cmd cmd_dir cmd_args name

syn keyword svFunc pre post svc_reload svc_restart svc_start svc_stop svc_status
syn cluster shCommandSubList add=svKwd

hi def link svKwd  Keyword
hi def link svFunc Special
hi def link svVars PreProc

let b:current_syntax = "sv"
