#include <stdio.h>
#include <stdlib.h>
double evaluate(const char *const expr)
{
    double result = 0.0;
    char op = '+';
    double value = 0.0;
    int i = 0;
    while (expr[i] != '\0')
    {
        if (expr[i] == '+' || expr[i] == '-' || expr[i] == '*' || expr[i] == '/')
        {
            op = expr[i];
            i++;
        }
        else if (expr[i] == '(')
        {
            char *sub_expr;
            int j = i + 1;
            int count = 1;
            while (count != 0)
            {
                if (expr[j] == '(')
                {
                    count++;
                }
                else if (expr[j] == ')')
                {
                    count--;
                }
                j++;
            }
            sub_expr = (char *)malloc(sizeof(char) * (j - i - 1));
            int k = 0;
            for (int l = i + 1; l < j - 1; l++)
            {
                sub_expr[k++] = expr[l];
            }
            sub_expr[k] = '\0';
            value = evaluate(sub_expr);
            free(sub_expr);
            switch (op)
            {
            case '+':
                result = result + value;
                break;
            case '-':
                result = result - value;
                break;
            case '*':
                result = result * value;
                break;
            case '/':
                result = result / value;
                break;
            }
            i = j;
        }
        else if (expr[i] >= '0' && expr[i] <= '9')
        {
            value = 0.0;
            while (expr[i] >= '0' && expr[i] <= '9')
            {
                value = value * 10 + (expr[i] - '0');
                i++;
            }
            if (expr[i] == '.')
            {
                double factor = 0.1;
                i++;
                while (expr[i] >= '0' && expr[i] <= '9')
                {
                    value = value + (expr[i] - '0') * factor;
                    factor = factor / 10;
                    i++;
                }
            }
            switch (op)
            {
            case '+':
                result = result + value;
                break;
            case '-':
                result = result - value;
                break;
            case '*':
                result = result * value;
                break;
            case '/':
                result = result / value;
                break;
            }
        }
        else
        {
            i++;
        }
    }
    return result;
}
int main()
{
    
    for(int i=0;i<100;i++){
        printf("1+");
    }
}