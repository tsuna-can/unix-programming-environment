def hanoi(n, src, via, dst):
    if( n == 1 ):
        print ("from ", src, " to ", dst, " : ", n)
    else:
        # 1 から n-1 までを出発地から中継地に移動
        hanoi(n-1, src, dst, via)
        # n を目的地に移動
        print ("from ", src, " to ", dst, " : ", n)
        # 1 から n-1 までを中継地から目的地に移動
        hanoi(n-1, via, src, dst)
hanoi(3, "A", "B", "C")
