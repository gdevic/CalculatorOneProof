#include <cmath>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>

#define MAX_MANT 14
#define MAX_SCRATCH  (MAX_MANT + 2)

static_assert(MAX_SCRATCH >= (MAX_MANT + 1), "MAX_SCRATCH needs to be at least (MAX_MANT + 1)");

extern uint32_t tests_total;
extern uint32_t tests_pass;
extern uint32_t tests_fail;

// Structure that abstracts a (normalized) register
typedef struct TReg
{
    // Registers will use BCD nibbles, but here we use chars
    char mant[MAX_MANT + 1]; // +1 to store a terminating zero; hw will not have that
    bool sign; // Set to true for negative mantissa
    uint8_t exps; // 8-bit exponent with a bias of 128

    TReg() : sign(false), exps(128), src(""), fp(0)
    {
        std::memset(mant, '0', MAX_MANT);
        mant[MAX_MANT] = 0;
    }

    // Constructor to use when loading a register with the user input buffer (from the input parser)
    TReg(const char *in) : TReg()
    {
        if (strlen(in) != 16)
            std::cerr << "Unexpected str size of " << strlen(in) << __FUNCTION__ << ":" << __LINE__ << "\n";
        src = in;
        read_fp_from_src();
        format_verif_from_fp();
    }

    // Constructor to use when creating a new register for computation
    TReg(double f) : TReg()
    {
        fp = f;
        format_verif_from_fp();
    }

    // Copy constructor
    TReg(const TReg &r) : sign(r.sign), exps(r.exps), src(r.src), fp(r.fp)
    {
        memcpy(mant, r.mant, MAX_MANT + 1);
        verif << r.verif.str();
    }

    // Given the source input buffer, reads a floating point number into the verification member variable (fp)
    void read_fp_from_src()
    {
        // Verification is done using a type double, which has 15 decimal digits of precision
        if (sscanf(src, "%lf", &fp) != 1)
            std::cerr << "Error reading src in " << __FUNCTION__ << ":" << __LINE__ << "\n";
        if ((src[11] == ' ') && (src[12] == 'E')) // If spaced away, compute the exponent separately
        {
            int pow;
            if (sscanf(&src[13], "%d", &pow) != 1)
                std::cerr << "Error reading exponent in " << __FUNCTION__ << ":" << __LINE__ << "\n";
            fp *= std::pow(10, pow);
        }
    }

    // Prints out the verification value (fp) into the string
    void format_verif_from_fp()
    {
        verif << ((*(long long *) &fp & 0x8000000000000000ll) ? "" : "+"); // Echo "+" for positive numbers
        verif.precision(MAX_MANT - 1); // Set the output precision, the number of digits, minus the first digit before '.'
        verif << std::scientific << fp;
    }

    // Prints out the register value, verification control value and whether they match or not
    void print(int id = 0)
    {
        std::ostringstream native;
        int pow = (exps & 0x80) ? exps & 0x7F : (128 + (~exps + 1)) & 0x7F;
        native << (sign ? "-" : "+");
        native << mant[0] << "." << &mant[1];
        native << ((exps & 0x80) ? "e+" : "e-");
        native << std::setfill('0') << std::setw(2) << pow;

        // XXX Signal for division by zero
        if (exps == 0)
            native.str(sign ? "-inf" : "+inf");

        // We want to detect implicit imprecision caused by rounding errors of the control fp
        // verification variable versus our native truncated result. If the both values match
        // (as string-compare of their prints) we display 'OK'; otherwise, it is either 'FAIL' or 'NEAR'
        // depending on the magnitude of a mismatch
        double native_fp;
        if (sscanf(native.str().c_str(), "%lf", &native_fp) != 1)
            std::cerr << "Error reading native_fp in " << __FUNCTION__ << ":" << __LINE__ << "\n";

        // Detect a rounding error equivalent to the magnitude of the last digit of the mantissa
        double max_diff = std::pow(10, -(MAX_MANT - 2));
        double diff = fabs(native_fp - fp);
        diff *= std::pow(10, -pow);
        bool rounding_error = diff <= max_diff;

        printf("%s = %c%s E%c%02d (%3d) %4d  ", src, sign ? '-' : '+', mant, (exps & 0x80) ? '+' : '-', pow, exps, id);
        std::cout << native.str() << " vs. " << verif.str() << "  ";
        if (native.str() == verif.str())
            std::cout << "OK\n", tests_pass++;
        else
            std::cout << (rounding_error ? "NEAR" : "FAIL") << " (" << diff << ")" << "\n", tests_fail += !rounding_error;
        tests_total++;
    }

    const char *src; // Source input string, used in print
    double fp{}; // For verification, "double" should have matching 15 digits of precision
    std::ostringstream verif;
} TREG;

// Structure that abstracts an arithmetic scratch register
typedef struct TAsr
{
    // Registers will use BCD nibbles, but here we use chars
    char mant[MAX_SCRATCH + 1]; // +1 to store a terminating zero; hw will not have that

    TAsr()
    {
        // Construct a scratch register with an invalid value to detect if an algorithm does not properly clear it
        std::memset(mant, 'X', MAX_SCRATCH);
        mant[MAX_SCRATCH] = 0;
    }

    TAsr(TREG &r) : TAsr()
    {
        std::memcpy(mant, r.mant, MAX_MANT); // Copy the mantissa of a register
        std::memset(mant + MAX_MANT, '0', MAX_SCRATCH - MAX_MANT); // Clear the extra nibbles
    }

} TASR;
