"
" Language:	Supervision
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

syn keyword svKwd begin end error eval_colors info yesno warn SOURCE
syn keyword svKwd env_del env_svc env_sv svc_cmd svc_down svc_status
syn keyword svKwd env_rs rs_cmd svc_sig svc_wait svc_zap mountinfo fstabinfo
syn keyword svKwd start start_pre start_post stop stop_pre stop_post restart
syn keyword svKwd log_start_pre log_stop_post ENV_DEL ENV_SET ENV_SVC
syn keyword svKwd checkpath fstabinfo mountinfo waitfile
syn keyword svKwd svc_status_start svc_status_stop svc_status_active svc_status_up
syn keyword svKwd svc_status_down svc_status_exist svc_status_pid
syn keyword svVars LOG_MODE LOG_PROC LOG_SIZE LOG_STAT LOG_PREFIX LOG_PRE_CMD
syn keyword svVars LOG_CMD LOG_OPTS LOG_FIN_CMD LOG_FIN_OPTS LOG_PRE_OPTS
syn keyword svVars LOGDIR LOG_ARGS LOG_STAT LOG_COMP LOG_PREFIX LOG_SIZE 
syn keyword svVars SVC_REQUIRED_FILES SVC_TIMEOUT_DOWN SVC_TIMEOUT_UP
syn keyword svVars SVC_DEBUG SVC_CONFIGFILE SVC_CMD SVC_OPTS SVC_GROUP SVC_USER
syn keyword svVars SVC_NEED SVC_USE SVC_BEFORE SVC_AFTER SV_TRY SVC_PROVIDE
syn keyword svVars ENV_DIR ENV_CMD ENV_OPTS PRE_CMD PRE_OPTS FIN_CMD FIN_OPTS
syn keyword svVars CGROUP_CLEANUP CGROUP_INHERIT SV_CGROUP SVC_DEPS SVC_PIDFILE
syn keyword svVars NULL SV_SVCDIR SV_TMPDIR SV_RUNDIR SV_SERVICE SV_OPTS SV_CMD
syn keyword svVars SVC_COMMANDS SVC_STARTED_COMMANDS SVC_STOPPED_COMMANDS
syn keyword svVars RC_OPTS COLOR description __cmd__ __cmd_dir__ __cmd_args__ name
syn keyword svVars SVC_KEYWORD SVC_TIMEOUT SVC_TRY SVC_SYSLOG SVC_SYSLOG_INFO
syn keyword svVars SVC_NICE SVC_CONFIGDIRS

syn cluster shCommandSubList add=svKwd

hi def link svKwd  Keyword
hi def link svFunc Special
hi def link svVars PreProc

let b:current_syntax = "sv"
