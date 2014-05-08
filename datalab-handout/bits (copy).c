/* 
 * CS:APP Data Lab 
 * 
 * Bin Feng bfeng
 * 
 * bits.c - Source file with your solutions to the Lab.
 *          This is the file you will hand in to your instructor.
 *
 * WARNING: Do not include the <stdio.h> header; it confuses the dlc
 * compiler. You can still use printf for debugging without including
 * <stdio.h>, although you might get a compiler warning. In general,
 * it's not good practice to ignore compiler warnings, but in this
 * case it's OK.  
 */

#if 0
/*
 * Instructions to Students:
 *
 * STEP 1: Read the following instructions carefully.
 */

You will provide your solution to the Data Lab byddd
editing the collection of functions in this source file.

INTEGER CODING RULES:
 
  Replace the "return" statement in each function with one
  or more lines of C code that implements the function. Your code 
  must conform to the following style:
 
  int Funct(arg1, arg2, ...) {
      /* brief description of how your implementation works */
      int var1 = Expr1;
      ...
      int varM = ExprM;

      varJ = ExprJ;
      ...
      varN = ExprN;
      return ExprR;
  }

  Each "Expr" is an expression using ONLY the following:
  1. Integer constants 0 through 255 (0xFF), inclusive. You are
      not allowed to use big constants such as 0xffffffff.
  2. Function arguments and local variables (no global variables).
  3. Unary integer operations ! ~
  4. Binary integer operations & ^ | + << >>
    
  Some of the problems restrict the set of allowed operators even further.
  Each "Expr" may consist of multiple operators. You are not restricted to
  one operator per line.

  You are expressly forbidden to:
  1. Use any control constructs such as if, do, while, for, switch, etc.
  2. Define or use any macros.
  3. Define any additional functions in this file.
  4. Call any functions.
  5. Use any other operations, such as &&, ||, -, or ?:
  6. Use any form of casting.
  7. Use any data type other than int.  This implies that you
     cannot use arrays, structs, or unions.

 
  You may assume that your machine:
  1. Uses 2s complement, 32-bit representations of integers.
  2. Performs right shifts arithmetically.
  3. Has unpredictable behavior when shifting an integer by more
     than the word size.

EXAMPLES OF ACCEPTABLE CODING STYLE:
  /*
   * pow2plus1 - returns 2^x + 1, where 0 <= x <= 31
   */
  int pow2plus1(int x) {
     /* exploit ability of shifts to compute powers of 2 */
     return (1 << x) + 1;
  }

  /*
   * pow2plus4 - returns 2^x + 4, where 0 <= x <= 31
   */
  int pow2plus4(int x) {
     /* exploit ability of shifts to compute powers of 2 */
     int result = (1 << x);
     result += 4;
     return result;
  }

FLOATING POINT CODING RULES

For the problems that require you to implent floating-point operations,
the coding rules are less strict.  You are allowed to use looping and
conditional control.  You are allowed to use both ints and unsigneds.
You can use arbitrary integer and unsigned constants.

You are expressly forbidden to:
  1. Define or use any macros.
  2. Define any additional functions in this file.
  3. Call any functions.
  4. Use any form of casting.
  5. Use any data type other than int or unsigned.  This means that you
     cannot use arrays, structs, or unions.
  6. Use any floating point data types, operations, or constants.


NOTES:
  1. Use the dlc (data lab checker) compiler (described in the handout) to 
     check the legality of your solutions.
  2. Each function has a maximum number of operators (! ~ & ^ | + << >>)
     that you are allowed to use for your implementation of the function. 
     The max operator count is checked by dlc. Note that '=' is not 
     counted; you may use as many of these as you want without penalty.
  3. Use the btest test harness to check your functions for correctness.
  4. Use the BDD checker to formally verify your functions
  5. The maximum number of ops for each function is given in the
     header comment for each function. If there are any inconsistencies 
     between the maximum ops in the writeup and in this file, consider
     this file the authoritative source.

/*
 * STEP 2: Modify the following functions according the coding rules.
 * 
 *   IMPORTANT. TO AVOID GRADING SURPRISES:
 *   1. Use the dlc compiler to check that your solutions conform
 *      to the coding rules.
 *   2. Use the BDD checker to formally verify that your solutions produce 
 *      the correct answers.
 */


#endif
/* 
 * bitAnd - x&y using only ~ and | 
 *   Example: bitAnd(6, 5) = 4
 *   Legal ops: ~ |
 *   Max ops: 8
 *   Rating: 1
 */
int bitAnd(int x, int y) {
  return ~(~x|~y);
}


/* 
 * copyLSB - set all bits of result to least significant bit of x
 *   Example: copyLSB(5) = 0xFFFFFFFF, copyLSB(6) = 0x00000000
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 5
 *   Rating: 2
 */
int copyLSB(int x) {

  // int tmp = x & 1;
  //printf("%d\n", (tmp<<31)>>31);
  return (x<<31)>>31;
}


/* 
 * leastBitPos - return a mask that marks the position of the
 *               least significant 1 bit. If x == 0, return 0
 *   Example: leastBitPos(96) = 0x20
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 6
 *   Rating: 2 
 */
int leastBitPos(int x) {

  int negX = ~x+1;
  return x & negX;
}


/* 
 * logicalShift - shift x to the right by n, using a logical shift
 *   Can assume that 0 <= n <= 31
 *   Examples: logicalShift(0x87654321,4) = 0x08765432
 *   Legal ops: ~ & ^ | + << >>
 *   Max ops: 20
 *   Rating: 3 
 */
int logicalShift(int x, int n) {

  int tmp1 = 1 << 31;

  // In order to compensate, we need to add
  // int tmp2 = ~(tmp1>>n) + (1<<(31+(~n+1)));
  // printf("n:%d\n", n);
  // int tmp2 = 1<<(32-n);
  // printf("tmp2:%d\n", tmp2);
  // printf("tmp2=%x\n", tmp2);

  // Work around for tmp1 >> (n-1)
  tmp1 = (tmp1 >> n) << 1;
  // printf("tmp1:%x\n", tmp1);
  tmp1 = ~tmp1;
  return (x>>n)&tmp1;
}



/*
 * bitCount - returns count of number of 1's in word
 *   Examples: bitCount(5) = 2, bitCount(7) = 3
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 40
 *   Rating: 4
 */
int bitCount(int x) {

  int mask = 0x11;
  int s = 0;
  int mask2 = 0;
  int res = 0;

  mask = (mask << 8) | mask;
  mask = (mask << 16) | mask; //mask=0001 0001 0001 0001 0001 0001 0001 0001
  // printf("%x\n", mask);
  s = mask & x;
  s = s + (mask & (x >> 1));
  s = s + (mask & (x >> 2));
  s = s + (mask & (x >> 3));
  s = s + (s >> 16);
  mask2 = 0xf;
  res = s & mask2;
  res = res + ((s >> 4) & mask2);
  res = res + ((s >> 8) & mask2);
  res = res + ((s >> 12) & mask2);
  return res;
}


/* 
 * TMax - return maximum two's complement integer 
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 4
 *   Rating: 1
 */
int tmax(void) {
  // int tmp = 1;
  // return (1<<31)+(~tmp+1);
  return ~(0x1<<31);
}


/* 
 * divpwr2 - Compute x/(2^n), for 0 <= n <= 30
 *  Round toward zero
 *   Examples: divpwr2(15,1) = 7, divpwr2(-33,4) = -2
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 15
 *   Rating: 2
 */
int divpwr2(int x, int n) {

    // (1<<n)+~0 == (1<<n)-1
    int balance = (x>>31)&((1<<n)+~0);
    // printf("balance:%d\n", (x>>31)&((1<<n)+~0));
    // printf("final:%d\n", ((x+balance)>>n));

    // When x is negative, balance need to be calculated
    return ((x+balance)>>n);
}


/* 
 * isNonNegative - return 1 if x >= 0, return 0 otherwise 
 *   Example: isNonNegative(-1) = 0.  isNonNegative(0) = 1.
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 6
 *   Rating: 3
 */
int isNonNegative(int x) {
  // printf("test:%x\n", x>>31);
  // return ((x>>31)&1^1);
  return !(x>>31);
}


/* 
 * isGreater - if x > y  then return 1, else return 0 
 *   Example: isGreater(4,5) = 0, isGreater(5,4) = 1
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 24
 *   Rating: 3
 */
int isGreater(int x, int y) {

  int signX = x >> 31;
  int signY = y >> 31;

  int signNotEqual = signX & (!signY);
  int signEqual = (!(signX^signY)) & ((x+~y)>>31);
  return !(signEqual | signNotEqual);
}


/* 
 * absVal - absolute value of x
 *   Example: absVal(-1) = 1.
 *   You may assume -TMax <= x <= TMax
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 10
 *   Rating: 4
 */
int absVal(int x) {

  int tmp = x >> 31;
  // printf("%x\n", (x^tmp)+(~tmp+1));
  // return (x^tmp)+(~tmp+1);
  return (x^tmp)+(tmp&0x01);
}


/*
 * isPower2 - returns 1 if x is a power of 2, and 0 otherwise
 *   Examples: isPower2(5) = 0, isPower2(8) = 1, isPower2(0) = 0
 *   Note that no negative number is a power of 2.
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 20
 *   Rating: 4
 */
int isPower2(int x) {

  // printf("T1=%d\n", !(x>>31));
  // printf("T2=%d\n", (x&(x+(~tmp+1))));
  // printf("T3=%d\n", !x);
  // printf("T4=%d\n", (x&(x+(~tmp+1))) + !x);
  // printf("T5=%d\n", !((x&(x+(~tmp+1))) + !x));
  // !x used to fix 0 input
  return !(x>>31) & !((x&(x+(~0)))+!x);

}


/* 
 * float_i2f - Return bit-level equivalent of expression (float) x
 *   Result is returned as unsigned int, but
 *   it is to be interpreted as the bit-level representation of a
 *   single-precision floating point values.
 *   Legal ops: Any integer/unsigned operations incl. ||, &&. also if, while
 *   Max ops: 30
 *   Rating: 4
 */
unsigned float_i2f(int x) {

  printf("x:%x\n");
  unsigned res = x & (1 << 31); /* set sign bit */
  int i;
  int exp = (x >> 31) ? 158 : 0; /* 158 = 31 + 127, INT_MIN or zero */
  int frac = 0;
  int delta;
  int frac_mask;

  if (x << 1) { /* x is neither 0 nor INT_MIN */
    if (x < 0)
      x = -x;
    i = 30;
    
    while ( !((x >> i) & 1) ) /* low 31 bits are always have 1(s) */
      i--;
    exp = i + 127;
    x = x << (31 - i);
    frac_mask = (1 << 23) - 1;
    frac = frac_mask & (x >> 8);
    x = x & 0xff;
    delta = x > 128 || ((x == 128) && (frac & 1));
    frac += delta;
    if(frac >> 23) {
      frac &= frac_mask;
      exp += 1;
    }
  }
  res = res | (exp << 23);
  res = res | frac;
  printf("res:%x\n");
  return res;
}


/* 
 * float_abs - Return bit-level equivalent of absolute value of f for
 *   floating point argument f.
 *   Both the argument and result are passed as unsigned int's, but
 *   they are to be interpreted as the bit-level representations of
 *   single-precision floating point values.
 *   When argument is NaN, return argument..
 *   Legal ops: Any integer/unsigned operations incl. ||, &&. also if, while
 *   Max ops: 10
 *   Rating: 2
 */
unsigned float_abs(unsigned uf) {
  
  int mantissa, exp, res;  
  mantissa = uf&0x007fffff;  
  exp = ((uf&0x7f800000)>>23);
  res = uf & 0x7fffffff;
  // int expOnes = 0x1;

  // printf("mantissa:%x\n", mantissa);
  // printf("exp:%x\n", exp);
  // printf("test:%x\n", ((expOnes<<31)>>31));
  if(exp==0xff){
    // printf("Br1\n");
    if(mantissa){
      return uf;
    }
    else{
      return res;
    }
    
  }
  else{
    // printf("Br3\n");
    return res;
  }
}
