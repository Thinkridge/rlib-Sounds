
export * from './core'

export const greet = (name: string): string => {
    return `Hello, ${name}! From my-ts-lib.`;
};

export const add = (a: number, b: number): number => {
    return a + b;
};
