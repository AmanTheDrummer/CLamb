#define main clamb_main
#include "clamb_lexer.c"
#undef main
int main(void){
    init_char_classes(); init_transition_table();
    for(int s=0;s<NUM_STATES;s++) for(int c=0;c<NUM_CLASSES;c++){
        Transition t=table[s][c];
        printf("%d %d %d %d %d %d %s\n",s,c,t.next_state,t.append,t.pushback,t.accept,
               t.accept?(t.type==TOK_DYNAMIC_IDENT?"dynamic":clamb_token_type_name(t.type)):"-");
    }
    return 0;
}