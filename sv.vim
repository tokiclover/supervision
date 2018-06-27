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

syn keyword svKwd begin die end error eval_colors info yesno checkpath warn SOURCE
syn keyword svKwd env_del env_svc env_sv  svc_cmd svc_down svc_mark svc_state
syn keyword svKwd env_rs rs_cmd svc_sig svc_wait svc_zap mountinfo fstabinfo
syn keyword svKwd start start_pre start_post stop stop_pre stop_post restart
syn keyword svKwd log_start_pre log_stop_post
syn keyword svVars LOG_MODE LOG_PROC LOG_SIZE LOG_STAT LOG_PREFIX
syn keyword svVars SVC_DEBUG SVC_CONFIGFILE SVC_CMD SVC_OPTS SVC_GROUP SVC_USER
syn keyword svVars SVC_USE SVC_NEED SVC_BEFORE SVC_AFTER SV_TRY SVC_PROVIDE
syn keyword svVars ENV_DIR ENV_CMD ENV_OPTS PRE_CMD PRE_OPTS FIN_CMD FIN_OPTS
syn keyword svVars LOG_CMD LOG_OPTS LOG_FIN_CMD LOG_FIN_OPTS SVC_NEED RC_OPTS
syn keyword svVars CGROUP_CLEANUP CGROUP_INHERIT SV_CGROUP SVC_DEPS SVC_PIDFILE
syn keyword svVars NULL SV_SVCDIR SV_TMPDIR SV_RUNDIR SV_SERVICE SV_OPTS SV_CMD
syn keyword svVars LOGDIR LOG_ARGS LOG_STAT LOG_COMP LOG_PREFIX LOG_SIZE SVC_COMMANDS
syn keyword svVars LOG_PRE_CMD LOG_PRE_OPTS SVC_REQUIRED_FILES SVC_WAIT_DOWN SVC_WAIT_UP
syn keyword svVars COLOR LOGDIR SVC_STARTED_COMMANDS SVC_STOPPED_COMMANDS
syn keyword svVars description __cmd__ __cmd_dir__ __cmd_args__ name
syn keyword svVars SVC_KEYWORD SVC_NOHANG SVC_TIMEOUT SVC_TRY

syn cluster shCommandSubList add=svKwd

hi def link svKwd  Keyword
hi def link svFunc Special
hi def link svVars PreProc

let b:current_syntax = "sv"
