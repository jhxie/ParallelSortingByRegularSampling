# ========================= GDB INITIALIZATION FILE ===========================

# ------------------------------ OPTION FLAGS ---------------------------------
set print pretty on
# set environment VARIABLE=CONTENT
# ------------------------------ OPTION FLAGS ---------------------------------

# ---------------------------- MACRO DEFINITIONS ------------------------------
# macro definition, show all user defined macros with "show user"
define pstrlen
printf "The length of the string is %d\n", $(arg0)
end
# ---------------------------- MACRO DEFINITIONS ------------------------------

# tbreak src/main.c:main
